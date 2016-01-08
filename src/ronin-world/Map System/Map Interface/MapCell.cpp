/***
 * Demonstrike Core
 */

//
// MapCell.cpp
//
#include "StdAfx.h"
MapCell::MapCell()
{
    _forcedActive = false;
}

MapCell::~MapCell()
{
    RemoveObjects();
}

void MapCell::Init(uint32 x, uint32 y, uint32 mapid, MapInstance* instance)
{
    _mapmgr = instance;
    _active = false;
    _loaded = false;
    _playerCount = 0;
    _x = x;
    _y = y;
    _unloadpending=false;
    _objects.clear();
}

void MapCell::AddObject(WorldObject* obj)
{
    if(obj->IsPlayer())
        ++_playerCount;

    _objects.insert(obj);
}

void MapCell::RemoveObject(WorldObject* obj)
{
    if(obj->IsPlayer())
        --_playerCount;

    _objects.erase(obj);
}

void MapCell::SetActivity(bool state)
{
    uint32 x = _x/8, y = _y/8;
    if(!_active && state)
    {
        // Move all objects to active set.
        for(CellObjectSet::iterator itr = _objects.begin(); itr != _objects.end(); itr++)
        {
            if(!(*itr)->Active && (*itr)->CanActivate())
                (*itr)->Activate(_mapmgr);
        }

        if(_unloadpending)
            CancelPendingUnload();
    }
    else if(_active && !state)
    {
        // Move all objects from active set.
        for(CellObjectSet::iterator itr = _objects.begin(); itr != _objects.end(); itr++)
        {
            if((*itr)->Active)
                (*itr)->Deactivate(_mapmgr);
        }

        if(!_unloadpending)
            QueueUnloadPending();
    }

    _active = state;
}

void MapCell::RemoveObjects()
{
    if(_loaded == false)
        return;

    _loaded = false;
    if(_respawnObjects.size())
    {
        /* delete objects in pending respawn state */
        WorldObject* pObject;
        for(CellObjectSet::iterator itr = _respawnObjects.begin(); itr != _respawnObjects.end(); itr++)
        {
            pObject = *itr;
            if(!pObject)
                continue;

            switch(pObject->GetTypeId())
            {
            case TYPEID_UNIT:
                {
                    if( !pObject->IsPet() )
                    {
                        _mapmgr->_reusable_guids_creature.push_back( pObject->GetLowGUID() );
                        castPtr<Creature>(pObject)->m_respawnCell = NULL;
                        castPtr<Creature>(pObject)->Destruct();
                        pObject = NULL;
                    }
                }break;
            case TYPEID_GAMEOBJECT:
                {
                    castPtr<GameObject>(pObject)->m_respawnCell = NULL;
                    castPtr<GameObject>(pObject)->Destruct();
                    pObject = NULL;
                }break;
            default:
                pObject->Destruct();
                pObject = NULL;
                break;
            }
        }
        _respawnObjects.clear();
    }

    if(_objects.size())
    {
        //This time it's simpler! We just remove everything :)
        WorldObject* obj; //do this outside the loop!
        for(CellObjectSet::iterator itr = _objects.begin(); itr != _objects.end();)
        {
            obj = (*itr);
            ++itr;

            if(!obj)
                continue;

            if( _unloadpending )
            {
                if(obj->GetHighGUID() == HIGHGUID_TYPE_TRANSPORTER)
                    continue;

                if(obj->GetTypeId() == TYPEID_CORPSE && obj->GetUInt32Value(CORPSE_FIELD_OWNER) != 0)
                    continue;
            }

            if( obj->IsInWorld())
                obj->RemoveFromWorld( true );

            obj->Destruct();
            obj = NULL;
        }
        _objects.clear();
    }

    _playerCount = 0;
}

uint32 MapCell::LoadObjects(CellSpawns * sp)
{
    if(_loaded == true)
        return 0;

    _loaded = true;
    uint32 loadCount = 0, mapId = _mapmgr->GetMapId();
    //MapInstance *pInstance = NULL;//_mapmgr->IsInstance() ? castPtr<InstanceMgr>(_mapmgr) : NULL;
    if(sp->CreatureSpawns.size())//got creatures
    {
        for(CreatureSpawnList::iterator i=sp->CreatureSpawns.begin();i!=sp->CreatureSpawns.end();++i)
        {
            CreatureSpawn *spawn = *i;
            /*if(pInstance && pInstance->m_killedNpcs.find(spawn->id) != pInstance->m_killedNpcs.end())
                continue;*/

            if(Creature *c = _mapmgr->CreateCreature(spawn->entry))
            {
                c->Load(mapId, spawn->x, spawn->y, spawn->z, spawn->o, _mapmgr->iInstanceMode, spawn);
                c->SetInstanceID(_mapmgr->GetInstanceID());
                if(!c->CanAddToWorld())
                {
                    c->Destruct();
                    continue;
                }

                c->PushToWorld(_mapmgr);
                loadCount++;
            }
        }
    }

    if(sp->GOSpawns.size())//got GOs
    {
        for(GOSpawnList::iterator i = sp->GOSpawns.begin(); i != sp->GOSpawns.end(); i++)
        {
            if(GameObject *go = _mapmgr->CreateGameObject((*i)->entry))
            {
                if(!go->Load(mapId, *i))
                {
                    go->Destruct();
                    continue;
                }
                go->PushToWorld(_mapmgr);
                loadCount++;
                TRIGGER_GO_EVENT(go, OnSpawn);
            }
        }
    }
    return loadCount;
}

void MapCell::QueueUnloadPending()
{
    if(_unloadpending)
        return;

    _unloadpending = true;
    sLog.Debug("MapCell", "Queueing pending unload of cell %u %u", _x, _y);
    sEventMgr.AddEvent(_mapmgr, &MapInstance::UnloadCell, (uint32)_x, (uint32)_y, MAKE_CELL_EVENT(_x,_y), 60000, 1, 0);
}

void MapCell::CancelPendingUnload()
{
    sLog.Debug("MapCell", "Cancelling pending unload of cell %u %u", _x, _y);
    if(!_unloadpending)
        return;

    sEventMgr.RemoveEvents(_mapmgr, MAKE_CELL_EVENT(_x,_y));
}

void MapCell::Unload()
{
    sLog.Debug("MapCell", "Unloading cell %u %u", _x, _y);
    ASSERT(_unloadpending);
    if(_active)
        return;

    RemoveObjects();
    _unloadpending = false;
}
