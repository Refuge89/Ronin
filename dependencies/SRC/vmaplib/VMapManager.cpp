/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2014-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <g3dlite\G3D.h>
#include "VMapLib.h"
#include "VMapDefinitions.h"

#include <iomanip>

using G3D::Vector3;

namespace VMAP
{
    VMapManager::VMapManager(const char* objDir) : vMapObjDir(objDir)
    {
        if(vMapObjDir.length() > 0 && vMapObjDir.at(vMapObjDir.length()-1) != '\\' && vMapObjDir.at(vMapObjDir.length()-1) != '/')
            vMapObjDir.push_back('/');
    }

    VMapManager::~VMapManager(void)
    {
        iDummyMaps.clear();
        GOModelList.clear();

        for (InstanceTreeMap::iterator i = iInstanceMapTrees.begin(); i != iInstanceMapTrees.end(); ++i)
            delete i->second;

        for (DynamicTreeMap::iterator i = iDynamicMapTrees.begin(); i != iDynamicMapTrees.end(); ++i)
            for(SubDynamicTreeMap::iterator it = i->second.begin(); it != i->second.end(); it++)
                delete it->second;

        for (ModelFileMap::iterator i = iLoadedModelFiles.begin(); i != iLoadedModelFiles.end(); ++i)
            delete i->second;
    }

    Vector3 VMapManager::convertPositionToInternalRep(float x, float y, float z)
    {
        static float mid = 0.5f * 64.0f * 533.33333333f;
        Vector3 pos(mid - x, mid - y, z);
        return pos;
    }

    bool VMapManager::loadMap(unsigned int mapId, FILE *file)
    {
        if(iDummyMaps.find(mapId) != iDummyMaps.end())
            return false;
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree == iInstanceMapTrees.end())
        {
            StaticMapTree* newTree = new StaticMapTree(mapId);
            if (!newTree->InitMap(this, file))
            {
                iDummyMaps.insert(mapId);
                delete newTree;
                return false;
            }

            iInstanceMapTrees.insert(InstanceTreeMap::value_type(mapId, newTree));
        }
        return true;
    }

    void VMapManager::unloadMap(unsigned int mapId)
    {
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            instanceTree->second->UnloadMap(this);
            if (instanceTree->second->numLoadedTiles() == 0)
            {
                delete instanceTree->second;
                iInstanceMapTrees.erase(mapId);
            }
        }
    }

    void VMapManager::unloadMap(unsigned int mapId, int x, int y)
    {
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree == iInstanceMapTrees.end())
            return;
        instanceTree->second->UnloadMapTile(x, y, this);
    }

    // load one tile (internal use only)
    bool VMapManager::loadMap(unsigned int mapId, int x, int y, FILE *file)
    {
        if(iDummyMaps.find(mapId) != iDummyMaps.end())
            return false;

        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree == iInstanceMapTrees.end())
            return false;

        return instanceTree->second->LoadMapTile(x, y, file, this);
    }

    // load one tile (internal use only)
    bool VMapManager::loadObject(G3D::uint64 guid, unsigned int mapId, G3D::uint32 DisplayID, float scale, float x, float y, float z, float o, G3D::uint32 instanceId, G3D::int32 m_phase)
    {
        if(iDummyMaps.find(mapId) != iDummyMaps.end())
            return false;

        SubDynamicTreeMap SDTM = iDynamicMapTrees[mapId];
        if(SDTM.find(instanceId) == SDTM.end())
            SDTM.insert(std::make_pair(instanceId, new DynamicMapTree()));

        return _loadObject(SDTM.at(instanceId), guid, mapId, DisplayID, scale, x, y, z, o, m_phase);
    }

    // Load our object into our dynamic tree(Internal only please!)
    bool VMapManager::_loadObject(DynamicMapTree* tree, G3D::uint64 guid, unsigned int mapId, G3D::uint32 DisplayID, float scale, float x, float y, float z, float o, G3D::int32 m_phase)
    {
        GOModelSpawnList::const_iterator it = GOModelList.find(DisplayID);
        if (it == GOModelList.end())
            return false;

        WorldModel* model = acquireModelInstance(it->second.name);
        if(model == NULL)
            return false; // Shouldn't happen.
        GameobjectModelInstance* Instance = new GameobjectModelInstance(it->second, model, m_phase);
        if(Instance == NULL)
        {
            releaseModelInstance(it->second.name);
            return false; // Shouldn't happen.
        }

        Instance->SetData(x, y, z, o, scale);
        GOMapGuides* guides = NULL;
        GOModelInstanceByGUID::iterator itr = GOModelTracker.find(mapId);
        if(GOModelTracker.find(mapId) == GOModelTracker.end())
        {
            guides = new GOMapGuides();
            GOModelTracker.insert(GOModelInstanceByGUID::value_type(mapId, guides));
        } else guides = GOModelTracker.at(mapId);

        guides->ModelsByGuid.insert(ModelGUIDEs::value_type(guid, Instance));
        tree->insert(*Instance);
        return true;
    }

    bool VMapManager::changeObjectModel(G3D::uint64 guid, unsigned int mapId, G3D::uint32 instanceId, G3D::uint32 DisplayID)
    {
        if(iDynamicMapTrees.find(mapId) == iDynamicMapTrees.end())
            return false;
        SubDynamicTreeMap SDTM = iDynamicMapTrees[mapId];
        if(SDTM.find(instanceId) == SDTM.end())
            return false;
        DynamicMapTree *tree = SDTM.at(instanceId);
        GOModelInstanceByGUID::iterator itr = GOModelTracker.find(mapId);
        if(itr == GOModelTracker.end())
            return false;

        GOMapGuides* guides = GOModelTracker.at(mapId);
        ModelGUIDEs::iterator itr2 = guides->ModelsByGuid.find(guid);
        if(itr2 == guides->ModelsByGuid.end())
            return false;

        GameobjectModelInstance *Instance = itr2->second;
        GOModelSpawnList::const_iterator it = GOModelList.find(DisplayID);
        if (it == GOModelList.end())
        {
            tree->remove(*Instance);
            Instance->setUnloaded();
            tree->insert(*Instance);
            return false;
        }
        else
        {
            G3D::AABox mdl_box(it->second.BoundBase);
            if (mdl_box == G3D::AABox::zero())
            {
                tree->remove(*Instance);
                Instance->setUnloaded();
                tree->insert(*Instance);
                return false;
            }

            WorldModel* model = acquireModelInstance(it->second.name);
            if(model == NULL)
            {
                tree->remove(*Instance);
                Instance->setUnloaded();
                tree->insert(*Instance);
                return false;
            }

            tree->remove(*Instance);
            G3D::Vector3 pos = Instance->getPosition();
            Instance->LoadModel(model, mdl_box);
            Instance->SetData(pos.x, pos.y, pos.z, Instance->GetOrientation(), Instance->GetScale());
            tree->insert(*Instance);
            return true;
        }
    }

    void VMapManager::unloadObject(unsigned int mapId, G3D::uint32 m_instance, G3D::uint64 guid)
    {
        GOModelInstanceByGUID::iterator itr = GOModelTracker.find(mapId);
        if(itr == GOModelTracker.end())
            return;
        GOMapGuides* guides = GOModelTracker.at(mapId);
        if(guides->ModelsByGuid.find(guid) != guides->ModelsByGuid.end())
        {
            DynamicTreeMap::iterator DynamicTree = iDynamicMapTrees.find(mapId);
            if (DynamicTree != iDynamicMapTrees.end() && DynamicTree->second.find(m_instance) != DynamicTree->second.end())
            {
                GameobjectModelInstance* Instance = guides->ModelsByGuid.at(guid);
                DynamicTree->second.at(m_instance)->remove(*Instance);
                releaseModelInstance(Instance->name);
                guides->ModelsByGuid.erase(guid);
                delete Instance;
            }
        }

        if(guides->ModelsByGuid.size() == 0)
        {
            GOModelTracker.erase(mapId);
            delete guides;
        }
    }

    bool VMapManager::isInLineOfSight(unsigned int mapId, G3D::uint32 instanceId, G3D::int32 m_phase, float x1, float y1, float z1, float x2, float y2, float z2)
    {
        bool result = true;
        if(G3D::fuzzyEq(x1, x2) && G3D::fuzzyEq(y1, y2) && G3D::fuzzyEq(z1, z2))
            return result; // Save us some time.

        Vector3 dest = convertPositionToInternalRep(x2, y2, z2);
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            Vector3 pos1 = convertPositionToInternalRep(x1, y1, z1);
            result = instanceTree->second->isInLineOfSight(pos1, dest);
        }
        dest = convertPositionToInternalRep(dest.x, dest.y, dest.z);

        // This is only a check for true or false so we only need to check when still in line of sight.
        if(result == true)
        {
            DynamicTreeMap::iterator DynamicTree = iDynamicMapTrees.find(mapId);
            if (DynamicTree != iDynamicMapTrees.end() && DynamicTree->second.find(instanceId) != DynamicTree->second.end())
                if(!DynamicTree->second.at(instanceId)->isInLineOfSight(x1, y1, z1, dest.x, dest.y, dest.z, m_phase))
                    result = false;
        }
        return result;
    }

    /**
    get the hit position and return true if we hit something
    otherwise the result pos will be the dest pos
    */
    bool VMapManager::getObjectHitPos(unsigned int mapId, G3D::uint32 instanceId, G3D::int32 m_phase, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float &ry, float& rz, float modifyDist)
    {
        bool result = false;
        rx = x2; ry = y2; rz = z2;
        if(G3D::fuzzyEq(x1, x2) && G3D::fuzzyEq(y1, y2) && G3D::fuzzyEq(z1, z2))
            return result; // Save us some time.

        Vector3 start = convertPositionToInternalRep(x1, y1, z1);
        Vector3 dest = convertPositionToInternalRep(x2, y2, z2);
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree != iInstanceMapTrees.end())
            result = instanceTree->second->getObjectHitPos(start, dest, dest, modifyDist);

        dest = convertPositionToInternalRep(dest.x, dest.y, dest.z);
        DynamicTreeMap::iterator DynamicTree = iDynamicMapTrees.find(mapId);
        if (DynamicTree != iDynamicMapTrees.end() && DynamicTree->second.find(instanceId) != DynamicTree->second.end())
        {
            start = convertPositionToInternalRep(start.x, start.y, start.z);
            result = DynamicTree->second.at(instanceId)->getObjectHitPos(start, dest, dest, modifyDist, m_phase);
            // No conversion needed for dynamic result
        }

        rx = dest.x;
        ry = dest.y;
        rz = dest.z;
        return result;
    }

    /**
    get height or INVALID_HEIGHT if no height available
    */

    float VMapManager::getHeight(unsigned int mapId, G3D::uint32 instanceId, G3D::int32 m_phase, float x, float y, float z, float maxSearchDist)
    {
        float height = VMAP_INVALID_HEIGHT;
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            Vector3 pos = convertPositionToInternalRep(x, y, z);
            float height2 = instanceTree->second->getHeight(pos, maxSearchDist);
            if (!G3D::fuzzyEq(height2, G3D::inf()))
                height = height2; // No height
        }

        DynamicTreeMap::iterator DynamicTree = iDynamicMapTrees.find(mapId);
        if (DynamicTree != iDynamicMapTrees.end() && DynamicTree->second.find(instanceId) != DynamicTree->second.end())
        {
            float height2 = DynamicTree->second.at(instanceId)->getHeight(x, y, z, maxSearchDist, m_phase);
            if (!G3D::fuzzyEq(height2, G3D::inf()))
                height = height2; // No height
        }

        return height;
    }

    G3D::uint32 VMapManager::getWMOData(unsigned int mapId, float x, float y, float z, G3D::uint32& wmoFlags, bool &areaResult, unsigned int &adtFlags, int& adtId, int& rootId, int& groupId, float &groundHeight, unsigned short &liquidFlags, float &liquidHeight, DataPointCallback* callback) const
    {
        InstanceTreeMap::const_iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            // Convert to internal position(negative is overlayed)
            Vector3 pos = convertPositionToInternalRep(x, y, z);
            // Grab our WMO data
            WMOData data;
            instanceTree->second->getWMOData(pos, data);
            if(data.groundResult) // Ground height
                groundHeight = data.ground_CalcZ;

            // Area result
            if(areaResult = data.hitResult)
            {
                adtFlags = data.flags;
                // Area data
                adtId = data.adtId;
                rootId = data.rootId;
                groupId = data.groupId;
                if(data.hitInstance)
                {
                    if((data.hitModel->GetMogpFlags() & WMO_FLAG_HAS_WMO_LIQUID) && data.hitInstance->GetLiquidLevel(pos, data.hitModel, liquidHeight))
                        liquidFlags = data.hitModel->GetLiquidType();
                    else if(!G3D::fuzzyEq(data.LiquidHeightSearch, -G3D::inf()))
                    {
                        liquidHeight = data.LiquidHeightSearch;
                        liquidFlags = data.liqTypeSearch;
                    }

                    if(data.hitModel->IsWithinObject(pos, data.hitInstance))
                        wmoFlags = data.hitModel->GetMogpFlags();
                    return data.rootId;
                }
            }
            else if(!G3D::fuzzyEq(data.LiquidHeightSearch, -G3D::inf()))
            {
                liquidHeight = data.LiquidHeightSearch;
                liquidFlags = data.liqTypeSearch;
            }
        }
        return 0;
    }

    bool VMapManager::getAreaInfo(unsigned int mapId, float x, float y, float& z, G3D::uint32& flags, G3D::int32& adtId, G3D::int32& rootId, G3D::int32& groupId) const
    {
        InstanceTreeMap::const_iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            Vector3 pos = convertPositionToInternalRep(x, y, z);
            bool result = instanceTree->second->getAreaInfo(pos, flags, adtId, rootId, groupId);
            // z is not touched by convertPositionToInternalRep(), so just copy
            z = pos.z;
            return result;
        }

        return false;
    }

    void VMapManager::getLiquidData(G3D::uint32 mapId, float x, float y, float z, G3D::uint16 &typeFlags, float &level) const
    {
        InstanceTreeMap::const_iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            LocationInfo info;
            Vector3 pos = convertPositionToInternalRep(x, y, z);
            if (instanceTree->second->GetLocationInfo(pos, info) && info.hitInstance->GetLiquidLevel(pos, info.hitModel, level))
                typeFlags = info.hitModel->GetLiquidType();
        }
    }

    WorldModel* VMapManager::acquireModelInstance(std::string filename)
    {
        //! Critical section, thread safe access to iLoadedModelFiles
        ManagedModel *retContainer = NULL;
        filenameMutexlock.lock();
        if(iModelNameLocks.find(filename) == iModelNameLocks.end())
            iModelNameLocks.insert(std::make_pair(filename, std::make_pair(0, new G3D::GMutex())));
        G3D::GMutex *lock = iModelNameLocks[filename].second;
        iModelNameLocks[filename].first++;
        filenameMutexlock.unlock();
        lock->lock();

        LoadedModelFilesLock.lock();
        if(DisabledModels.find(filename) == DisabledModels.end())
        {
            ModelFileMap::iterator model = iLoadedModelFiles.find(filename);
            if(model == iLoadedModelFiles.end())
            {
                retContainer = new ManagedModel(new WorldModel());
                LoadedModelFilesLock.unlock();

                bool res = retContainer->getModel()->readFile(vMapObjDir + filename + ".vmo");
                LoadedModelFilesLock.lock();
                if(res == true)
                {
                    OUT_DEBUG("VMapManager: loading file '%s%s'", vMapObjDir.c_str(), filename.c_str());
                    model = iLoadedModelFiles.insert(std::make_pair(filename, retContainer)).first;
                }
                else
                {
                    OUT_DEBUG("VMapManager: could not load '%s%s.vmo'!", vMapObjDir.c_str(), filename.c_str());
                    DisabledModels.insert(filename);
                    retContainer->invalidate();
                    if(retContainer->decRefCount() == 0)
                        delete retContainer;
                    retContainer = NULL;
                }
            }

            if(model != iLoadedModelFiles.end())
                (retContainer = model->second)->incRefCount();
        }
        LoadedModelFilesLock.unlock();

        filenameMutexlock.lock();
        lock->unlock();
        if((--iModelNameLocks[filename].first) == 0)
        {
            iModelNameLocks[filename].second = NULL;
            iModelNameLocks.erase(filename);
            delete lock;
        }
        filenameMutexlock.unlock();
        return retContainer ? retContainer->getModel() : NULL;
    }

    void VMapManager::releaseModelInstance(std::string filename)
    {
        filenameMutexlock.lock();
        if(iModelNameLocks.find(filename) == iModelNameLocks.end())
            iModelNameLocks.insert(std::make_pair(filename, std::make_pair(0, new G3D::GMutex())));
        G3D::GMutex *lock = iModelNameLocks[filename].second;
        iModelNameLocks[filename].first++;
        filenameMutexlock.unlock();
        lock->lock();

        //! Critical section, thread safe access to iLoadedModelFiles
        LoadedModelFilesLock.lock();
        ModelFileMap::iterator model;
        if ((model = iLoadedModelFiles.find(filename)) != iLoadedModelFiles.end())
        {
            if (model->second->decRefCount() == 0)
            {
                OUT_DEBUG("VMapManager: unloading file '%s'", filename.c_str());
                ManagedModel* mModel = model->second;
                iLoadedModelFiles.erase(model);
                delete mModel;
            }
        }
        LoadedModelFilesLock.unlock();

        filenameMutexlock.lock();
        lock->unlock();
        if((--iModelNameLocks[filename].first) == 0)
        {
            iModelNameLocks[filename].second = NULL;
            iModelNameLocks.erase(filename);
            delete lock;
        }
        filenameMutexlock.unlock();
    }

    void VMapManager::updateDynamicMapTree(G3D::uint32 t_diff, G3D::int32 mapid)
    {
        if(iDynamicMapTrees.find(mapid) == iDynamicMapTrees.end())
            return;

        for(SubDynamicTreeMap::iterator itr = iDynamicMapTrees.at(mapid).begin(); itr != iDynamicMapTrees.at(mapid).end(); itr++)
            itr->second->update(t_diff);
    }

    void VMapManager::LoadGameObjectModelList()
    {
        FILE* model_list_file = fopen((vMapObjDir + GAMEOBJECT_MODELS).c_str(), "rb");
        if (!model_list_file)
        {
            OUT_DEBUG("Unable to open '%s' file.", GAMEOBJECT_MODELS);
            return;
        }

        G3D::uint32 name_length, displayId;
        char buff[500];
        while (true)
        {
            Vector3 v1, v2;
            if (fread(&displayId, sizeof(G3D::uint32), 1, model_list_file) != 1)
                if (feof(model_list_file))  // EOF flag is only set after failed reading attempt
                    break;

            if (fread(&name_length, sizeof(G3D::uint32), 1, model_list_file) != 1
                || name_length >= sizeof(buff)
                || fread(&buff, sizeof(char), name_length, model_list_file) != name_length
                || fread(&v1, sizeof(Vector3), 1, model_list_file) != 1
                || fread(&v2, sizeof(Vector3), 1, model_list_file) != 1)
            {
                OUT_DEBUG("File '%s' seems to be corrupted!", GAMEOBJECT_MODELS);
                break;
            }

            G3D::AABox box = G3D::AABox(v1, v2);
            if(box == G3D::AABox::zero())
            {
                // ignore models with no bounds
                OUT_DEBUG("GameObject model %s has zero bounds, loading skipped", buff);
                continue;
            }

            GameobjectModelSpawn iModel;
            iModel.name = std::string(buff, name_length);
            iModel.BoundBase = box;
            GOModelList.insert(GOModelSpawnList::value_type(displayId, iModel));
        }

        fclose(model_list_file);
        OUT_DETAIL("Loaded %u GameObject models", G3D::uint32(GOModelList.size()));
    }

} // namespace VMAP
