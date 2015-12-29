/***
 * Demonstrike Core
 */

//
// WorldCreator.cpp
//

#include "StdAfx.h"

initialiseSingleton( FormationMgr );

SERVER_DECL WorldManager sWorldMgr;

WorldManager::WorldManager()
{
    memset(m_maps, NULL, sizeof(Map*)* NUM_MAPS);
    memset(m_instances, NULL, sizeof(InstanceMap*) * NUM_MAPS);
    memset(m_singleMaps, NULL, sizeof(MapMgr*) * NUM_MAPS);
}

void WorldManager::Load(TaskList * l)
{
    new FormationMgr;
    new WorldStateTemplateManager;
    sWorldStateTemplateManager.LoadFromDB();
    QueryResult *result;
    // Create all non-instance type maps.
    result = CharacterDatabase.Query( "SELECT MAX(id) FROM instances" );
    if( result )
    {
        m_InstanceHigh = result->Fetch()[0].GetUInt32()+1;
        delete result;
    }
    else
        m_InstanceHigh = 1;

    // create maps for any we don't have yet.
    StorageContainerIterator<MapInfo> * itr = WorldMapInfoStorage.MakeIterator();
    while(!itr->AtEnd())
    {
        MapInfo* mapinfo = itr->Get();
        if( mapinfo->mapid >= NUM_MAPS )
        {
            sLog.Warning("WorldManager", "One or more of your worldmap_info rows specifies an invalid map: %u", mapinfo->mapid );
            itr->Inc();
            continue;
        }

        if(m_maps[mapinfo->mapid] == NULL)
            l->AddTask(new Task(new CallbackP1<WorldManager,uint32>(this, &WorldManager::_CreateMap, mapinfo->mapid)));

        if( mapinfo->flags != 1 && mapinfo->cooldown == 0) //Transport maps have 0 update_distance since you don't load into them ;)
        {
            sLog.Warning("WorldManager", "Your worldmap_info has no cooldown for map %u.", itr->Get()->mapid);
            itr->Get()->cooldown = TIME_MINUTE * 30;
        }
        itr->Inc();
    }
    itr->Destruct();
    l->wait();

    // load saved instances
    _LoadInstances();
}

void WorldManager::Load(uint32 mapid)
{
    QueryResult *result;
    sWorldStateTemplateManager.LoadFromDB(mapid);

    // Create all non-instance type maps.
    result = CharacterDatabase.Query( "SELECT MAX(id) FROM instances WHERE mapid = '%i'", mapid);
    if( result )
    {
        m_InstanceHigh = result->Fetch()[0].GetUInt32()+1;
        delete result;
    }
    else
        m_InstanceHigh = 1;

    _CreateMap(mapid);

    // load saved instances
    _LoadInstances();
}

WorldManager::~WorldManager()
{
    delete WorldStateTemplateManager::getSingletonPtr();
}

void WorldManager::Shutdown()
{
    uint32 i;
    InstanceMap::iterator itr;
    for(i = 0; i < NUM_MAPS; i++)
    {
        if(m_instances[i] != NULL)
        {
            for(itr = m_instances[i]->begin(); itr != m_instances[i]->end(); itr++)
            {
                itr->second->KillThread();
                delete itr->second;
            }

            delete m_instances[i];
            m_instances[i] = NULL;
        }

        if(m_singleMaps[i] != NULL)
        {
            MapMgr* ptr = m_singleMaps[i];
            ptr->KillThread();
            delete ptr;
            ptr = NULL;
            m_singleMaps[i] = NULL;// and it dies :)
        }

        if(m_maps[i] != NULL)
        {
            delete m_maps[i];
            m_maps[i] = NULL;
        }
    }

    delete FormationMgr::getSingletonPtr();
}

uint32 WorldManager::PreTeleport(uint32 mapid, Player* plr, uint32 instanceid)
{
    // preteleport is where all the magic happens :P instance creation, etc.
    MapInfo * inf = WorldMapInfoStorage.LookupEntry(mapid);
    MapEntry* map = dbcMap.LookupEntry(mapid);
    InstanceMap * instancemap;
    InstanceMgr * in = NULL;

    //is the map vaild?
    if(inf == NULL || mapid >= NUM_MAPS)
        return INSTANCE_ABORT_NOT_FOUND;

    // main continent check.
    if(inf->type == INSTANCE_NULL)
        return (m_singleMaps[mapid] != NULL) ? INSTANCE_OK : INSTANCE_ABORT_NOT_FOUND; // we can check if the destination world server is online or not and then cancel them before they load.

    // shouldn't happen
    if(inf->type == INSTANCE_PVP)
        return INSTANCE_ABORT_NOT_FOUND;

    if(map->IsRaid()) // check that heroic mode is available if the player has requested it.
    {
        if(plr->iRaidType > 1 && inf->type != INSTANCE_MULTIMODE)
            return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
    }
    else if(plr->iInstanceType && inf->type != INSTANCE_MULTIMODE)
        return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;

    //do we need addition raid/heroic checks?
    Group * pGroup = plr->GetGroup() ;
    if( !plr->triggerpass_cheat )
    {
        // players without groups cannot enter raid instances (no soloing them:P)
        if( pGroup == NULL && (map->IsRaid() || inf->type == INSTANCE_MULTIMODE))
            return INSTANCE_ABORT_NOT_IN_RAID_GROUP;

        //and has the required level
        if( plr->getLevel() < 80)
        {
            if(!map->IsRaid())
            {
                //otherwise we still need to be lvl 70/80 for heroic.
                if( plr->iInstanceType && plr->getLevel() < uint32(inf->HasFlag(WMI_INSTANCE_XPACK_02) ? 80 : 70))
                    return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
            }
            else
                //otherwise we still need to be lvl 70/80 for heroic.
                if( plr->iRaidType > 1 && plr->getLevel() < uint32(inf->HasFlag(WMI_INSTANCE_XPACK_02) ? 80 : 70))
                    return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;

            //and we might need a key too.
            bool reqkey = (inf->heroic_key[0] || inf->heroic_key[1])? true : false;
            bool haskey = (plr->GetInventory()->GetItemCount(inf->heroic_key[0]) || plr->GetInventory()->GetItemCount(inf->heroic_key[1]))? true : false;
            if(reqkey && !haskey)
                return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
        }
    }

    // if we are here, it means:
    // 1) we're a non-raid instance
    // 2) we're a raid instance, and the person is in a group.
    // so, first we have to check if they have an instance on this map already, if so, allow them to teleport to that.
    // next we check if there is a saved instance belonging to him.
    // otherwise, we can create them a new one.

    m_mapLock.Acquire();

    //find all instances for our map
    instancemap = m_instances[mapid];
    if(instancemap)
    {
        InstanceMap::iterator itr;
        // do we have a specific instance id we should enter (saved or active).
        // don't bother looking for saved instances, if we had one we found it in areatrigger.cpp
        if(instanceid != 0)
        {
            itr = instancemap->find(instanceid);
            if(itr != instancemap->end())
            {
                in = itr->second;
                //we have an instance,but can we enter it?
                uint8 owns = PlayerOwnsInstance( in, plr );
                if( owns >= OWNER_CHECK_OK )
                {
                    // If the map is active and has players
                    if(in->HasPlayers() && !plr->triggerpass_cheat)
                    {
                        //check if combat is in progress
                        if( in->IsCombatInProgress())
                        {
                            m_mapLock.Release();
                            return INSTANCE_ABORT_ENCOUNTER;
                        }

                        // check if we are full
                        if( in->GetPlayerCount() >= inf->playerlimit )
                        {
                            m_mapLock.Release();
                            return INSTANCE_ABORT_FULL;
                        }
                    }

                    uint32 plrdiff = map->IsRaid() ? plr->iRaidType : plr->iInstanceType;
                    if(in->iInstanceMode == plrdiff)
                    {
                        if(owns == OWNER_CHECK_SAVED_OK && !in->HasPlayers())
                        {
                            if(plr->GetGroup())
                                in->m_creatorGroup = plr->GetGroupID();
                        }

                        m_mapLock.Release();
                        if(owns == OWNER_CHECK_RESET_LOC)
                            return INSTANCE_OK_RESET_POS;
                        return INSTANCE_OK;
                    }
                    else
                    {
                        m_mapLock.Release();
                        return INSTANCE_ABORT_TOO_MANY;
                    }
                }
                else
                    sLog.Debug("WorldManager","Check failed %s, return code %u",plr->GetName(), owns);
            }
            m_mapLock.Release();
            return INSTANCE_ABORT_NOT_FOUND;
        }
        else
        {
            // search all active instances and see if we have one here.
            for(itr = instancemap->begin(); itr != instancemap->end();)
            {
                in = itr->second;
                ++itr;
                //we have an instance,but do we own it?
                uint8 owns = PlayerOwnsInstance(in, plr);
                if( owns >= OWNER_CHECK_OK )
                {
                    // check the player count and in combat status.
                    if(in->HasPlayers() && !plr->triggerpass_cheat)
                    {
                        if( in->IsCombatInProgress())
                        {
                            m_mapLock.Release();
                            return INSTANCE_ABORT_ENCOUNTER;
                        }

                        // check if we are full
                        if( in->GetPlayerCount() >= inf->playerlimit )
                        {
                            m_mapLock.Release();
                            return INSTANCE_ABORT_FULL;
                        }
                    }

                    uint32 plrdiff = map->IsRaid() ? plr->iRaidType : plr->iInstanceType;
                    if(in->iInstanceMode == plrdiff)
                    {
                        if(owns == OWNER_CHECK_SAVED_OK && !in->HasPlayers())
                        {
                            if(plr->GetGroup())
                                in->m_creatorGroup = plr->GetGroupID();
                        }

                        // found our instance, allow him in.
                        m_mapLock.Release();
                        if(owns == OWNER_CHECK_RESET_LOC)
                            return INSTANCE_OK_RESET_POS;
                        return INSTANCE_OK;
                    }
                }
                else
                    sLog.Debug("WorldManager","Check failed %s, return code %u",plr->GetName(), owns);
            }
        }
    }
    else
    {
        if(instanceid != 0)
        {
            // wtf, how can we have an instance_id for a mapid which doesn't even exist?
            m_mapLock.Release();
            return INSTANCE_ABORT_NOT_FOUND;
        }
        // this mapid hasn't been added yet, so we gotta create the hashmap now.
        m_instances[mapid] = new InstanceMap;
        instancemap = m_instances[mapid];
    }

    // if we're here, it means we need to create a new instance.
    bool raid = map->IsRaid();
    // create the actual instance (if we don't GetInstance() won't be able to access it).
    in = castPtr<InstanceMgr>(_CreateMapMgr(mapid, GenerateInstanceID(), true));
    in->m_creation = UNIXTIME;
    in->m_expiration = (raid ? UNIXTIME + inf->cooldown : 0);       // expire time 0 is 10 minutes after last player leaves
    in->m_creatorGuid = plr->GetLowGUID();
    in->m_creatorGroup = (pGroup ? pGroup->GetID() : 0);
    in->iInstanceMode = (raid ? plr->iRaidType : plr->iInstanceType);
    in->InactiveMoveTime = 60+UNIXTIME;

    //crash fix; GM's without group will start up raid instances as if they where nonraids
    //this to avoid exipring check, this is mainly for developers purpose; GM's should NOT invite any players here!
    if( plr->triggerpass_cheat && !plr->GetGroup() && raid)
    {
        const char * message = "Started this instance for development purposes only, do not invite players!!";
        sEventMgr.AddEvent( plr, &Player::_Warn, message, EVENT_UNIT_SENDMESSAGE, 5000, 1, 0);
    }

    in->m_isBattleground = false;
    plr->SetInstanceID(in->GetInstanceID());
    sLog.Debug("WorldManager", "Prepared new %s %u for player %u and group %u on map %u with difficulty %u.", raid ? "Raid" : "Instance" , in->GetInstanceID(), in->m_creatorGuid, in->m_creatorGroup, in->GetMapId(), in->iInstanceMode);

    // apply it in the instance map
    instancemap->insert( InstanceMap::value_type( in->GetInstanceID(), in ) );

    // instance created ok, i guess? return the ok for him to transport.
    m_mapLock.Release();
    return INSTANCE_OK_RESET_POS;
}

MapMgr* WorldManager::GetMapMgr(uint32 mapId)
{
    return m_singleMaps[mapId];
}

const uint32 GetBGForMapID(uint32 type)
{
    switch(type)
    {
    case 30:
        return BATTLEGROUND_ALTERAC_VALLEY;
    case 489:
        return BATTLEGROUND_WARSONG_GULCH;
    case 529:
        return BATTLEGROUND_ARATHI_BASIN;
    case 566:
        return BATTLEGROUND_EYE_OF_THE_STORM;
    case 607:
        return BATTLEGROUND_STRAND_OF_THE_ANCIENTS;
    case 628:
        return BATTLEGROUND_ISLE_OF_CONQUEST;
    case 559:
    case 562:
    case 572:
    case 617:
    case 618:
        return BATTLEGROUND_ARENA;
    }
    return 0;
};

bool WorldManager::PushToWorldQueue(WorldObject *obj)
{
    if(MapMgr* mapMgr = GetInstance(obj))
    {
        if(Player* p = obj->IsPlayer() ? castPtr<Player>(obj) : NULL)
        {
            // battleground checks
            if( p->m_bg == NULL && mapMgr->m_battleground != NULL )
            {
                // player hasn't been registered in the battleground, ok.
                // that means we re-logged into one. if it's an arena, don't allow it!
                // also, don't allow them in if the bg is full.
                if( !mapMgr->m_battleground->CanPlayerJoin(p) && !p->bGMTagOn)
                    return false;
            }

            // players who's group disbanded cannot remain in a raid instances alone(no soloing them:P)
            if( !p->triggerpass_cheat && p->GetGroup()== NULL && (mapMgr->GetdbcMap()->IsRaid() || mapMgr->GetMapInfo()->type == INSTANCE_MULTIMODE))
                return false;

            p->m_beingPushed = true;
            if(WorldSession *sess = p->GetSession())
                sess->SetInstance(mapMgr->GetInstanceID());
        }
        else if(Creature *c = obj->IsCreature() ? castPtr<Creature>(obj) : NULL)
            if(!c->CanAddToWorld())
                return false;

        mapMgr->AddObject(obj);
        return true;
    }
    return false;
}

MapMgr* WorldManager::GetInstance(WorldObject* obj)
{
    Player* plr = NULL;
    InstanceMap::iterator itr;
    InstanceMap * instancemap = NULL;
    MapInfo * inf = WorldMapInfoStorage.LookupEntry(obj->GetMapId());

    // we can *never* teleport to maps without a mapinfo.
    if( inf == NULL || obj->GetMapId() >= NUM_MAPS )
        return NULL;

    if( obj->IsPlayer() )
    {
        // players can join instances based on their groups/solo status.
        plr = castPtr<Player>( obj );

        m_mapLock.Acquire();
        instancemap = m_instances[obj->GetMapId()];
        // single-instance maps never go into the instance set.
        if( inf->type == INSTANCE_NULL )
        {
            if(instancemap == NULL || instancemap->find(obj->GetInstanceID()) == instancemap->end())
            {
                if(!(GetBGForMapID(obj->GetMapId()) && plr->m_bg != NULL))
                {
                    m_mapLock.Release();
                    return m_singleMaps[obj->GetMapId()];
                }
            }
        }

        if(instancemap != NULL)
        {
            // check our saved instance id. see if its valid, and if we can join before trying to find one.
            itr = instancemap->find(obj->GetInstanceID());
            if(itr != instancemap->end())
            {
                if(itr->second)
                {
                    //we have an instance,but can we enter it?
                    uint8 owns =  PlayerOwnsInstance( itr->second, plr );
                    if( owns >= OWNER_CHECK_OK )
                    {
                        if(owns == OWNER_CHECK_SAVED_OK && !itr->second->HasPlayers())
                        {
                            if(plr->GetGroup())
                                itr->second->m_creatorGroup = plr->GetGroupID();
                        }
                        m_mapLock.Release();
                        return itr->second;
                    }
                }
            }

            // iterate over our instances, and see if any of them are owned/joinable by him.
            for(itr = instancemap->begin(); itr != instancemap->end(); itr++)
            {
                // Is this our instance?
                uint8 owns = PlayerOwnsInstance(itr->second, plr);
                if(owns >= OWNER_CHECK_OK )
                {
                    if(owns == OWNER_CHECK_SAVED_OK && !itr->second->HasPlayers())
                        if(plr->GetGroup())
                            itr->second->m_creatorGroup = plr->GetGroupID();

                    m_mapLock.Release();
                    if(owns == OWNER_CHECK_RESET_LOC)
                        return NULL;
                    return itr->second;
                }
                else
                    sLog.Debug("WorldManager","Check failed %u %s", owns, plr->GetName());
            }
        }

        // if we're here, it means there are no instances on that map, or none of the instances on that map are joinable
        // by this player.
        m_mapLock.Release();
        return NULL;
    }
    else
    {
        m_mapLock.Acquire();
        instancemap = m_instances[obj->GetMapId()];
        if(instancemap != NULL)
        {
            itr = instancemap->find(obj->GetInstanceID());
            if(itr != instancemap->end())
            {
                // we never create instances just for units.
                m_mapLock.Release();
                return itr->second;
            }
        }

        // units are *always* limited to their set instance ids.
        if(inf->type == INSTANCE_NULL)
        {
            m_mapLock.Release();
            return m_singleMaps[obj->GetMapId()];
        }

        // instance is non-existant (shouldn't really happen for units...)
        m_mapLock.Release();
        return NULL;
    }
}

MapMgr* WorldManager::GetInstance(uint32 MapId, uint32 InstanceId)
{
    InstanceMgr * in;
    InstanceMap::iterator itr;
    InstanceMap * instancemap;
    MapInfo * inf = WorldMapInfoStorage.LookupEntry(MapId);

    // we can *never* teleport to maps without a mapinfo.
    if( inf == NULL || MapId >= NUM_MAPS )
        return NULL;

    // single-instance maps never go into the instance set.
    if( inf->type == INSTANCE_NULL )
        return m_singleMaps[MapId];

    m_mapLock.Acquire();
    instancemap = m_instances[MapId];
    if(instancemap != NULL)
    {
        // check our saved instance id. see if its valid, and if we can join before trying to find one.
        itr = instancemap->find(InstanceId);
        if(itr != instancemap->end())
        {
            if(itr->second)
            {
                m_mapLock.Release();
                return itr->second;
            }
        }

        // iterate over our instances, and see if any of them are owned/joinable by him.
        for(itr = instancemap->begin(); itr != instancemap->end();)
        {
            in = itr->second;
            ++itr;

            // instance is already created.
            m_mapLock.Release();
            return in;
        }
    }

    // if we're here, it means there are no instances on that map, or none of the instances on that map are joinable
    // by this player.
    m_mapLock.Release();
    return NULL;
}

MapMgr* WorldManager::_CreateMapMgr(uint32 mapid, uint32 instanceid, bool instance)
{
    MapInfo* inf = WorldMapInfoStorage.LookupEntry(mapid);
    ASSERT(inf != NULL && inf->type == INSTANCE_NULL);
    ASSERT(mapid < NUM_MAPS && m_maps[mapid] != NULL);

    MapMgr* ret = instance ? new InstanceMgr(m_maps[mapid], mapid, instanceid) : new MapMgr(m_maps[mapid], mapid, instanceid);
    ret->Init(instance);
    if(instance == false)
    {
        const char* name = m_maps[mapid]->GetName();
        bool transportmap = (strstr(name, "Transport") ? true : false);
        if(transportmap) // Only list transports on debug.
            sLog.Debug("WorldManager", "Created transport map %u.", mapid);
        else if(m_maps[mapid]->IsCollisionEnabled())
            sLog.Notice("WorldManager", "Created Collision continent %s.", name);
        else sLog.Notice("WorldManager", "Created continent %s.", name);

        // assign pointer
        m_singleMaps[mapid] = ret;
    }

    // start its thread
    ThreadPool.ExecuteTask(format("Map mgr - M%u|I%u", mapid, instanceid).c_str(), ret);
    return ret;
}

InstanceMgr * WorldManager::GetSavedInstance(uint32 map_id, uint32 guid, uint32 difficulty)
{
    InstanceMap::iterator itr;
    InstanceMap * instancemap;

    m_mapLock.Acquire();
    instancemap = m_instances[map_id];
    if(instancemap)
    {
        for(itr = instancemap->begin(); itr != instancemap->end();)
        {
            if(itr != instancemap->end())
            {
                itr->second->m_SavedLock.Acquire();
                if(itr->second->iInstanceMode == difficulty)
                {
                    if( itr->second->m_SavedPlayers.find(guid) != itr->second->m_SavedPlayers.end() )
                    {
                        itr->second->m_SavedLock.Release();
                        m_mapLock.Release();
                        return itr->second;
                    }
                }
                itr->second->m_SavedLock.Release();
                ++itr;
            }
        }
    }
    m_mapLock.Release();
    return NULL;
}

void WorldManager::_CreateMap(uint32 mapid)
{
    if( mapid >= NUM_MAPS )
        return;

    MapInfo* inf = WorldMapInfoStorage.LookupEntry(mapid);
    if(!inf || m_maps[mapid])
        return;

    m_maps[mapid] = new Map(mapid, inf);
    if(sWorld.ServerPreloading)
        m_maps[mapid]->LoadAllTerrain();

    if(inf->type == INSTANCE_NULL)
    {
        // we're a continent, create the instance.
        _CreateMapMgr(mapid, GenerateInstanceID());
    }
}

uint32 WorldManager::GenerateInstanceID()
{
    uint32 iid;
    m_mapLock.Acquire();
    iid = m_InstanceHigh++;
    m_mapLock.Release();
    return iid;
}

void BuildStats(MapMgr* mgr, char * m_file)
{
    MapInfo * inf = mgr->GetMapInfo();
    MapEntry *me = dbcMap.LookupEntry(inf->mapid);
    if (me == NULL)
        return;
    InstanceMgr *inst = mgr->IsInstance() ? castPtr<InstanceMgr>(mgr) : NULL;

    char tmp[200];
    strcpy(tmp, "");
#define pushline strcat(m_file, tmp)

    snprintf(tmp, 200, "    <instance>\n");
    pushline;
    snprintf(tmp, 200, "        <map>%u</map>\n", mgr->GetMapId());
    pushline;
    snprintf(tmp, 200, "        <instanceid>%u</instanceid>\n", mgr->GetInstanceID());
    pushline;
    snprintf(tmp, 200, "        <maptype>%u</maptype>\n", inf->type);
    pushline;
    snprintf(tmp, 200, "        <players>%u</players>\n", (unsigned int)mgr->GetPlayerCount());
    pushline;
    snprintf(tmp, 200, "        <maxplayers>%u</maxplayers>\n", inf->playerlimit);
    pushline;

    //<creationtime>
    if (inst)
    {
        tm *ttime = localtime( &inst->m_creation );
        snprintf(tmp, 200, "        <creationtime>%02u:%02u:%02u %02u/%02u/%u</creationtime>\n",ttime->tm_hour, ttime->tm_min, ttime->tm_sec, ttime->tm_mday, ttime->tm_mon, uint32( ttime->tm_year + 1900 ));
        pushline;
    }
    else
    {
        snprintf(tmp, 200, "        <creationtime>N/A</creationtime>\n");
        pushline;
    }

    //<expirytime>
    if (inst && inst->m_expiration)
    {
        tm *ttime = localtime( &inst->m_expiration );
        snprintf(tmp, 200, "        <expirytime>%02u:%02u:%02u %02u/%02u/%u</expirytime>\n",ttime->tm_hour, ttime->tm_min, ttime->tm_sec, ttime->tm_mday, ttime->tm_mon, uint32( ttime->tm_year + 1900 ));
        pushline;
    }
    else
    {
        snprintf(tmp, 200, "        <expirytime>N/A</expirytime>\n");
        pushline;

    }
    //<idletime>
    if (mgr->InactiveMoveTime)
    {
        tm *ttime = localtime( &mgr->InactiveMoveTime );
        snprintf(tmp, 200, "        <idletime>%02u:%02u:%02u %02u/%02u/%u</idletime>\n",ttime->tm_hour, ttime->tm_min, ttime->tm_sec, ttime->tm_mday, ttime->tm_mon, uint32( ttime->tm_year + 1900 ));
        pushline;
    }
    else
    {
        snprintf(tmp, 200, "        <idletime>N/A</idletime>\n");
        pushline;
    }

    snprintf(tmp, 200, "    </instance>\n");                                                                                                            pushline;
#undef pushline
}

void WorldManager::BuildXMLStats(char * m_file)
{
    uint32 i;
    InstanceMap::iterator itr;
    InstanceMap * instancemap;
    InstanceMgr * in;

    m_mapLock.Acquire();
    for(i = 0; i < NUM_MAPS; i++)
    {
        if(m_singleMaps[i] != NULL)
            BuildStats(m_singleMaps[i], m_file);
    }
    // Crow: WE CAN MAKE THIS BETTER, FASTER, STRONGER!!!

    for(i = 0; i < NUM_MAPS; i++)
    {
        if(m_singleMaps[i] == NULL)
        {
            instancemap = m_instances[i];
            if(instancemap != NULL)
            {
                for(itr = instancemap->begin(); itr != instancemap->end();)
                {
                    in = itr->second;
                    ++itr;

                    BuildStats(in, m_file);
                }
            }
        }
    }

    m_mapLock.Release();
    sLog.Debug("WorldManager", "Dumping XML stats...");
}

void WorldManager::_LoadInstances()
{
    MapInfo* inf;
    InstanceMgr* in;
    QueryResult* result;

    // clear any instances that have expired.
    sLog.Notice("WorldManager", "Deleting Expired Instances...");
    CharacterDatabase.WaitExecute("DELETE FROM instances WHERE expiration <= %u", UNIXTIME);

    // load saved instances
    result = CharacterDatabase.Query("SELECT * FROM instances");
    sLog.Notice("WorldManager", "Loading %u saved instance(s)." , result ? result->GetRowCount() : 0);

    if(result)
    {
        do
        {
            inf = WorldMapInfoStorage.LookupEntry(result->Fetch()[1].GetUInt32());
            if(inf == NULL || result->Fetch()[1].GetUInt32() >= NUM_MAPS)
            {
                CharacterDatabase.Execute("DELETE FROM instances WHERE mapid = %u", result->Fetch()[1].GetUInt32());
                continue;
            }

            in = castPtr<InstanceMgr>(_CreateMapMgr(inf->mapid, result->Fetch()[0].GetUInt32(), true));
            in->LoadFromDB(result->Fetch());

            if(m_instances[inf->mapid] == NULL)
                m_instances[inf->mapid] = new InstanceMap;

            m_instances[inf->mapid]->insert( InstanceMap::value_type( in->GetInstanceID(), in ) );
        } while(result->NextRow());
        sLog.Success("WorldManager", "Loaded %u saved instance(s)." , result->GetRowCount());
        delete result;
    } else sLog.Debug("WorldManager", "No saved instances found.");
}

void InstanceMgr::LoadFromDB(Field * fields)
{
    m_creation = fields[2].GetUInt32();
    m_expiration = fields[3].GetUInt32();
    iInstanceMode = fields[5].GetUInt32();
    m_creatorGroup = fields[6].GetUInt32();
    m_creatorGuid = fields[7].GetUInt32();
    m_isBattleground = false;

    // process saved npc's
    char * pnpcstr, *npcstr;
    char * m_npcstring = strdup(fields[4].GetString());

    npcstr = m_npcstring;
    pnpcstr = strchr(m_npcstring, ' ');
    while(pnpcstr)
    {
        *pnpcstr = 0;
        uint32 val = atol(npcstr);
        if (val)
            m_killedNpcs.insert( val );
        npcstr = pnpcstr+1;
        pnpcstr = strchr(npcstr, ' ');
    }
    free(m_npcstring);

    // process saved players
    char * pplayerstr, *playerstr;
    char * m_playerstring = strdup(fields[8].GetString());
    playerstr = m_playerstring;
    pplayerstr = strchr(m_playerstring, ' ');
    while(pplayerstr)
    {
        *pplayerstr = 0;
        uint32 val = atol(playerstr);
        if (val) // No mutex required here, we are only calling this during start up.
            m_SavedPlayers.insert( val );
        playerstr = pplayerstr+1;
        pplayerstr = strchr(playerstr, ' ');
    }
    free(m_playerstring);
}

void WorldManager::ResetSavedInstances(Player* plr)
{
    if(plr == NULL || !plr->IsInWorld() || plr->IsInInstance())
        return;

    InstanceMgr * in;
    bool found = false;
    InstanceMap::iterator itr;

    m_mapLock.Acquire();
    for(uint32 i = 0; i < NUM_MAPS; i++)
    {
        if(m_instances[i] != NULL)
        {
            MapEntry* map = dbcMap.LookupEntry(i);
            InstanceMap* instancemap = m_instances[i];
            for(itr = instancemap->begin(); itr != instancemap->end();)
            {
                in = itr->second;
                ++itr;

                if(!map->IsRaid() && plr->GetGroupID() == in->m_creatorGroup)
                {
                    found = true;
                    if( in->iInstanceMode == MODE_5PLAYER_HEROIC && in->m_SavedPlayers.size() )//heroic instances can't be reset once they are saved.
                    {
                        plr->GetSession()->SystemMessage("Heroic instances are reset daily at 08:00 CET!");
                        continue;
                    }

                    if(plr->GetGroup() != NULL && plr->GetGroup()->GetLeader() != plr->m_playerInfo)
                    {
                        plr->GetSession()->SystemMessage("Can't reset instance %u (%s), you are not the group leader!", in->GetInstanceID(), in->GetMapInfo()->mapName);
                        continue;
                    }

                    if(in->HasPlayers())
                    {
                        plr->GetSession()->SystemMessage("Can't reset instance %u (%s) when there are still players inside!", in->GetInstanceID(), in->GetMapInfo()->mapName);
                        continue;
                    }

                    // destroy the instance
                    if(_DeleteInstance(in, true, false))
                    {
                        // <mapid> has been reset.
                        WorldPacket data(SMSG_INSTANCE_RESET, 4);
                        data << uint32(in->GetMapId());
                        plr->GetSession()->SendPacket(&data);
                    }
                }
            }
        }
    }
    m_mapLock.Release();

    if(found == false)
        plr->GetSession()->SystemMessage("No instances found.");
}

void WorldManager::ResetHeroicInstances()
{
    // checking for any expired instances.
    InstanceMgr * in;
    InstanceMap::iterator itr;
    InstanceMap * instancemap;
    uint32 i;

    m_mapLock.Acquire();
    for(i = 0; i < NUM_MAPS; i++)
    {
        instancemap = m_instances[i];
        if(instancemap)
        {
            for(itr = instancemap->begin(); itr != instancemap->end();)
            {
                in = itr->second;
                ++itr;

                if(!in->GetdbcMap()->IsRaid())
                {
                    // use a "soft" delete here.
                    if(in->iInstanceMode == MODE_5PLAYER_HEROIC)
                        _DeleteInstance(in, false, false);
                }
            }

        }
    }
    m_mapLock.Release();
}

bool WorldManager::_DeleteInstance(InstanceMgr * in, bool ForcePlayersOut, bool atSelfEnd)
{
    m_mapLock.Acquire();
    InstanceMap * instancemap;
    InstanceMap::iterator itr;

    // "ForcePlayersOut" will teleport the players in this instance to their entry point/ronin.
    // otherwise, they will get a 60 second timeout telling them they are not in this instance's group.
    if(in->HasPlayers())
    {
        if(ForcePlayersOut)
            in->OnShutdown();
        else in->BeginInstanceExpireCountdown();
    } else in->OnShutdown();

    // remove the instance from the large map.
    instancemap = m_instances[in->GetMapId()];
    if(instancemap)
    {
        itr = instancemap->find(in->GetInstanceID());
        if(itr != instancemap->end())
            instancemap->erase(itr);
    }

    // cleanup corpses, database references
    in->DeleteFromDB();
    if(atSelfEnd == false) // delete the instance pointer.
        delete in;
    m_mapLock.Release();
    return true;
}

void InstanceMgr::DeleteFromDB()
{
    // cleanup all the corpses on this map
    CharacterDatabase.Execute("DELETE FROM corpses WHERE instanceid = %u", GetInstanceID());

    // delete from the database
    CharacterDatabase.Execute("DELETE FROM instances WHERE id = %u", GetInstanceID());
}

void WorldManager::CheckForExpiredInstances()
{
    // checking for any expired instances.
    InstanceMgr * in;
    InstanceMap::iterator itr;
    InstanceMap * instancemap;
    uint32 i;

    m_mapLock.Acquire();
    for(i = 0; i < NUM_MAPS; i++)
    {
        instancemap = m_instances[i];
        if(instancemap)
        {
            for(itr = instancemap->begin(); itr != instancemap->end();)
            {
                in = itr->second;
                ++itr;

                // use a "soft" delete here.
                if(HasInstanceExpired(in))
                    _DeleteInstance(in, false, false);
            }
        }
    }
    m_mapLock.Release();
}

void WorldManager::BuildSavedInstancesForPlayer(Player* plr)
{
    std::set<uint32> mapIds;
    if(!plr->IsInWorld() || plr->GetMapMgr()->GetMapInfo()->type != INSTANCE_NULL)
    {
        m_mapLock.Acquire();
        for(uint32 i = 0; i < NUM_MAPS; i++)
        {
            if(m_instances[i] != NULL)
            {
                InstanceMap *instancemap = m_instances[i];
                for(InstanceMap::iterator itr = instancemap->begin(); itr != instancemap->end();)
                {
                    InstanceMgr *in = itr->second;
                    ++itr;

                    if( !in->GetdbcMap()->IsRaid() && (PlayerOwnsInstance(in, plr) >= OWNER_CHECK_OK) )
                    {
                        m_mapLock.Release();

                        mapIds.insert(in->GetMapId());
                        break; //next mapid
                    }
                }
            }
        }
        m_mapLock.Release();
    }

    WorldPacket data(SMSG_UPDATE_INSTANCE_OWNERSHIP, 4);
    data << uint32(mapIds.size() ? 0x01 : 0x00);
    plr->GetSession()->SendPacket(&data);

    for(std::set<uint32>::iterator itr = mapIds.begin(); itr != mapIds.end(); itr++)
        plr->GetSession()->OutPacket(SMSG_UPDATE_LAST_INSTANCE, 4, ((uint8*)*itr));
}

void WorldManager::BuildSavedRaidInstancesForPlayer(Player* plr)
{
    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 200);
    InstanceMgr * in;
    InstanceMap::iterator itr;
    uint32 i;
    uint32 counter = 0;

    data << counter;
    for(i = 0; i < NUM_MAPS; i++)
    {
        MapEntry* map = dbcMap.LookupEntry(i);
        if(map)
        {
            in = GetSavedInstance(i, plr->GetLowGUID(), plr->iRaidType);
            if(in && map->IsRaid())
            {
                data << in->GetMapId();
                data << in->iInstanceMode;
                data << uint64(in->GetInstanceID());
                data << uint8(in->m_expiration < UNIXTIME ? 0 : 1);
                data << uint8(0);
                if( in->m_expiration > UNIXTIME )
                    data << uint32(in->m_expiration - UNIXTIME);
                else
                    data << uint32(0);

                ++counter;
            }
        }
    }

    *(uint32*)&data.contents()[0] = counter;
    plr->GetSession()->SendPacket(&data);
}

void InstanceMgr::SaveToDB()
{
    // don't save non-raid instances.
    if(!dbcMap.LookupEntry(_mapId)->IsRaid() || m_isBattleground)
        return;

    // don't save instance if nothing is killed yet
    if (m_killedNpcs.size() == 0)
        return;

    // Add new players to existing m_SavedPlayers
    m_SavedLock.Acquire();
    MapMgr::PlayerStorageMap::iterator itr1 = m_PlayerStorage.begin();
    std::unordered_set<uint32>::iterator itr2, itr3;
    for(; itr1 != m_PlayerStorage.end(); itr1++)
    {
        itr2 = m_SavedPlayers.find(itr1->second->GetLowGUID());
        if( itr2 == m_SavedPlayers.end() )
            m_SavedPlayers.insert(itr1->second->GetLowGUID());
    }
    m_SavedLock.Release();

    std::stringstream ss;

    ss << "REPLACE INTO instances VALUES("
        << m_instanceID << ","
        << _mapId << ","
        << (uint32)m_creation << ","
        << (uint32)m_expiration << ",'";

    for(itr3 = m_killedNpcs.begin(); itr3 != m_killedNpcs.end(); itr3++)
        ss << (*itr3) << " ";

    ss  << "',"
        << iInstanceMode << ","
        << m_creatorGroup << ","
        << m_creatorGuid.getLow() << ",'";

    m_SavedLock.Acquire();
    for(itr2 = m_SavedPlayers.begin(); itr2 != m_SavedPlayers.end(); itr2++)
        ss << (*itr2) << " ";
    m_SavedLock.Release();

    ss <<"')";

    CharacterDatabase.Execute(ss.str().c_str());
}

void WorldManager::PlayerLeftGroup(Group * pGroup, Player* pPlayer)
{
    // does this group own any instances? we have to kick the player out of those instances.
    InstanceMgr * in;
    InstanceMap::iterator itr;
    InstanceMap * instancemap;
    WorldPacket data(SMSG_RAID_GROUP_ONLY, 8);
    uint32 i;

    m_mapLock.Acquire();
    for(i = 0; i < NUM_MAPS; i++)
    {
        instancemap = m_instances[i];
        if(instancemap)
        {
            for(itr = instancemap->begin(); itr != instancemap->end();)
            {
                in = itr->second;
                ++itr;

                bool eject = false;

                if(pPlayer->GetGUID() == in->m_creatorGuid)
                    eject = true;
                else if(pPlayer->GetGroupID() == in->m_creatorGroup)
                    eject = true;
                else
                {
                    // Are we on the saved list?
                    in->m_SavedLock.Acquire();
                    if( in->m_SavedPlayers.find(pPlayer->GetLowGUID()) != in->m_SavedPlayers.end() )
                        eject = true;
                    in->m_SavedLock.Release();
                }

                if(eject)
                {
                    // better make sure we're actually in that instance.. :P
                    if(!pPlayer->raidgrouponlysent && pPlayer->GetInstanceID() == (int32)in->GetInstanceID())
                    {
                        data << uint32(60000) << uint32(1);
                        pPlayer->GetSession()->SendPacket(&data);
                        pPlayer->raidgrouponlysent=true;

                        sEventMgr.AddEvent(pPlayer, &Player::EjectFromInstance, EVENT_PLAYER_EJECT_FROM_INSTANCE, 60000, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);

                        m_mapLock.Release();
                        return;
                    }
                }
            }
        }
    }
    m_mapLock.Release();
}

MapMgr* WorldManager::CreateBattlegroundInstance(uint32 mapid)
{
    // shouldn't happen
    if( mapid >= NUM_MAPS )
        return NULL;

    if(dbcMap.LookupEntry(mapid) == NULL)
        return NULL;

    if(!m_maps[mapid])
    {
        _CreateMap(mapid);
        if(!m_maps[mapid])
            return NULL;
    }

    InstanceMgr* ret = new InstanceMgr(m_maps[mapid], mapid, GenerateInstanceID());
    ret->Init(true);
    ret->m_creation = UNIXTIME;
    ret->m_creatorGroup = 0;
    ret->m_creatorGuid = 0;
    ret->iInstanceMode = MODE_5PLAYER_NORMAL;
    ret->m_expiration = 0;
    ret->m_isBattleground = true;
    m_mapLock.Acquire();
    if( m_instances[mapid] == NULL )
        m_instances[mapid] = new InstanceMap;

    m_instances[mapid]->insert( std::make_pair( ret->GetInstanceID(), ret ) );
    m_mapLock.Release();
    ThreadPool.ExecuteTask(format("BattleGround Mgr - M%u|I%u", mapid, ret->GetInstanceID()).c_str(), ret);
    return ret;
}

void WorldManager::DeleteBattlegroundInstance(uint32 mapid, uint32 instanceid)
{
    m_mapLock.Acquire();
    InstanceMap::iterator itr = m_instances[mapid]->find( instanceid );
    if( itr == m_instances[mapid]->end() )
    {
        printf("Could not delete battleground instance!\n");
        m_mapLock.Release();
        return;
    }

    m_instances[mapid]->erase( itr );
    m_mapLock.Release();
}

FormationMgr::FormationMgr()
{
    if(QueryResult * res = WorldDatabase.Query("SELECT * FROM creature_formations"))
    {
        do
        {
            uint32 formation = res->Fetch()[0].GetUInt32();
            if(m_formations.find(formation) != m_formations.end())
                continue;

            Formation *f = new Formation;
            f->fol = res->Fetch()[1].GetUInt32();
            f->ang = res->Fetch()[2].GetFloat();
            f->dist = res->Fetch()[3].GetFloat();
            m_formations.insert(std::make_pair(formation, f));
        } while(res->NextRow());
        delete res;
    }
}

FormationMgr::~FormationMgr()
{
    FormationMap::iterator itr;
    for(itr = m_formations.begin(); itr != m_formations.end(); itr++)
        delete itr->second;
}
