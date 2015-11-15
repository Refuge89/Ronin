/***
 * Demonstrike Core
 */

#include "StdAfx.h"

Object::Object(uint64 guid, uint32 fieldCount) : m_valuesCount(fieldCount), m_objGuid(guid), m_updateMask(m_valuesCount), m_inWorld(false), m_objectUpdated(false)
{
    m_updateMask.SetCount(m_valuesCount);
    m_uint32Values = new uint32[m_valuesCount];
    memset(m_uint32Values, 0, sizeof(uint32)*m_valuesCount);

    SetUInt64Value(OBJECT_FIELD_GUID, guid);
    SetFloatValue(OBJECT_FIELD_SCALE_X, 1.f);
    SetTypeFlags(TYPEMASK_TYPE_OBJECT);

    m_loot.gold = 0;
    m_looted = false;
}

Object::~Object()
{

}

void Object::Init()
{

}

void Object::Destruct()
{
    delete this;
}

void Object::SetByte(uint32 index, uint32 index1,uint8 value)
{
    ASSERT( index < m_valuesCount );

    if(index1 >= 4)
    {
        sLog.outError("Object::SetByteValue: wrong offset %u", index1);
        return;
    }

    if(uint8(m_uint32Values[ index ] >> (index1 * 8)) == value)
        return;

    m_uint32Values[ index ] &= ~uint32(uint32(0xFF) << (index1 * 8));
    m_uint32Values[ index ] |= uint32(uint32(value) << (index1 * 8));
    SetUpdateField(index);
}

void Object::SetByteFlag(const uint32 index, const uint32 flag, uint8 newFlag)
{
    if( HasByteFlag(index,flag,newFlag))
        return;

    SetByte(index, flag, GetByte(index,flag)|newFlag);
    SetUpdateField(index);
}

void Object::RemoveByteFlag(const uint32 index, const uint32 flag, uint8 checkFlag)
{
    if( !HasByteFlag(index,flag,checkFlag))
        return;

    SetByte(index,flag, GetByte(index,flag) & ~checkFlag );
    SetUpdateField(index);
}

bool Object::HasByteFlag(const uint32 index, const uint32 flag, uint8 checkFlag)
{
    if( GetByte(index,flag) & checkFlag )
        return true;
    return false;
}

void Object::SetUInt16Value(uint16 index, uint8 offset, uint16 value)
{
    ASSERT( index < m_valuesCount );
    if (uint16(m_uint32Values[index] >> (offset * 16)) == value)
        return;

    m_uint32Values[index] &= ~uint32(uint32(0xFFFF) << (offset * 16));
    m_uint32Values[index] |= uint32(uint32(value) << (offset * 16));
    SetUpdateField(index);
}

void Object::SetUInt32Value( const uint32 index, const uint32 value )
{
    ASSERT( index < m_valuesCount );
    if(m_uint32Values[index] == value)
        return;

    m_uint32Values[ index ] = value;
    SetUpdateField(index);
}

void Object::SetUInt64Value( const uint32 index, const uint64 value )
{
    assert( index + 1 < m_valuesCount );
    if(m_uint32Values[index] == ((uint32*)&value)[0] && m_uint32Values[index+1] == ((uint32*)&value)[1])
        return;

    m_uint32Values[ index ] = ((uint32*)&value)[0];
    m_uint32Values[ index + 1 ] = ((uint32*)&value)[1];

    SetUpdateField(index);
    SetUpdateField(index+1);
}

void Object::SetFlag( const uint32 index, uint32 newFlag )
{
    ASSERT( index < m_valuesCount );
    if(m_uint32Values[ index ] & newFlag)
        return;

    m_uint32Values[ index ] |= newFlag;
    SetUpdateField(index);
}

void Object::RemoveFlag( const uint32 index, uint32 oldFlag )
{
    ASSERT( index < m_valuesCount );
    if((m_uint32Values[ index ] & oldFlag) == 0)
        return;

    m_uint32Values[ index ] &= ~oldFlag;
    SetUpdateField(index);
}

void Object::SetFloatValue( const uint32 index, const float value )
{
    ASSERT( index < m_valuesCount );
    if(m_floatValues[index] == value)
        return;

    m_floatValues[ index ] = value;
    SetUpdateField(index);
}

void Object::ModFloatValue(const uint32 index, const float value )
{
    ASSERT( index < m_valuesCount );
    m_floatValues[ index ] += value;
    SetUpdateField(index);
}

void Object::ModSignedInt32Value(uint32 index, int32 value )
{
    ASSERT( index < m_valuesCount );
    if(value == 0)
        return;

    m_uint32Values[ index ] += value;
    SetUpdateField(index);
}

void Object::ModUnsigned32Value(uint32 index, int32 mod)
{
    ASSERT( index < m_valuesCount );
    if(mod == 0)
        return;

    m_uint32Values[ index ] += mod;
    if( (int32)m_uint32Values[index] < 0 )
        m_uint32Values[index] = 0;
    SetUpdateField(index);
}

uint32 Object::GetModPUInt32Value(const uint32 index, const int32 value)
{
    ASSERT( index < m_valuesCount );
    int32 basevalue = (int32)m_uint32Values[ index ];
    return ((basevalue*value)/100);
}

void Object::SetUpdateField(uint32 index)
{
    m_updateMask.SetBit( index );
    OnFieldUpdated(index);
}

bool Object::_SetUpdateBits(UpdateMask *updateMask, uint32 updateFlags)
{
    bool res = false;
    uint16 typeMask = GetTypeFlags(), offset = 0, *flags, fLen = 0;
    for(uint8 f = 0; f < 10; f++)
    {
        if(typeMask & 1<<f)
        {
            GetUpdateFieldData(f, flags, fLen);
            for(uint32 i = 0; i < fLen; i++, offset++)
            {
                if(!m_updateMask.GetBit(offset))
                    continue;
                if(flags[i] != 0 && (flags[i] & updateFlags) == 0)
                    continue;
                res = true;
                updateMask->SetBit(offset);
            }
        }
    }
    return res;
}

uint16 Object::GetUpdateFlag(Player *target)
{
    uint16 flag = UF_FLAG_PUBLIC + (target == this ? UF_FLAG_PRIVATE : 0);
    if(target)
    {
        switch (GetTypeId())
        {
        case TYPEID_UNIT:
        case TYPEID_PLAYER:
            {
                if (target->GetGUID() == castPtr<Unit>(this)->GetUInt64Value(UNIT_FIELD_SUMMONEDBY))
                    flag |= UF_FLAG_OWNER;
                else if (target->GetGUID() == castPtr<Unit>(this)->GetUInt64Value(UNIT_FIELD_CREATEDBY))
                    flag |= UF_FLAG_OWNER;
                if (IsPlayer() && castPtr<Player>(this)->InGroup() && castPtr<Player>(this)->GetGroupID() == target->GetGroupID())
                    flag |= UF_FLAG_PARTY_MEMBER;
            }break;
        case TYPEID_GAMEOBJECT:
            {
                if (target->GetGUID() == castPtr<GameObject>(this)->GetUInt64Value(GAMEOBJECT_FIELD_CREATED_BY))
                    flag |= UF_FLAG_OWNER;
            }break;
        case TYPEID_DYNAMICOBJECT:
            {
                if (target->GetGUID() == castPtr<DynamicObject>(this)->GetCasterGuid())
                    flag |= UF_FLAG_OWNER;
            }break;
        case TYPEID_CORPSE:
            {
                if (target->GetGUID() == castPtr<Corpse>(this)->GetUInt64Value(CORPSE_FIELD_OWNER))
                    flag |= UF_FLAG_OWNER;
            }break;
        }
    }

    return flag;
}

void Object::GetUpdateFieldData(uint8 type, uint16 *&flags, uint16 &length)
{
    switch (type)
    {
    case TYPEID_OBJECT: { length = OBJECT_LENGTH; flags = ObjectUpdateFieldFlags; }break;
    case TYPEID_ITEM: { length = ITEM_LENGTH; flags = ItemUpdateFieldFlags; }break;
    case TYPEID_CONTAINER: { length = CONTAINER_LENGTH; flags = ContainerUpdateFieldFlags; }break;
    case TYPEID_UNIT: { length = UNIT_LENGTH; flags = UnitUpdateFieldFlags; }break;
    case TYPEID_PLAYER: { length = PLAYER_LENGTH; flags = PlayerUpdateFieldFlags; }break;
    case TYPEID_GAMEOBJECT: { length = GAMEOBJECT_LENGTH; flags = GameObjectUpdateFieldFlags; }break;
    case TYPEID_DYNAMICOBJECT: { length = DYNAMICOBJECT_LENGTH; flags = DynamicObjectUpdateFieldFlags; }break;
    case TYPEID_CORPSE: { length = CORPSE_LENGTH; flags = CorpseUpdateFieldFlags; }break;
    case TYPEID_AREATRIGGER: { length = AREATRIGGER_LENGTH; flags = AreaTriggerUpdateFieldFlags; }break;
    }
}

uint32 Object::BuildCreateUpdateBlockForPlayer(ByteBuffer *data, Player* target)
{
    uint16 updateFlags = IsVehicle() ? UPDATEFLAG_VEHICLE : UPDATEFLAG_NONE;
    uint8 updatetype = UPDATETYPE_CREATE_OBJECT;
    if(GetTypeFlags() & TYPEMASK_TYPE_UNIT)
    {
        updateFlags |= UPDATEFLAG_LIVING;
        // Players or player linked units
        if(IsPlayer() || IsPet() || IsTotem() || IsSummon())
            updatetype = UPDATETYPE_CREATE_PLAYEROBJ;
    }
    else if(GetTypeFlags() & TYPEMASK_TYPE_GAMEOBJECT)
    {
        updateFlags = UPDATEFLAG_STATIONARY_POS|UPDATEFLAG_ROTATION;
        switch(GetByte(GAMEOBJECT_BYTES_1, GAMEOBJECT_BYTES_TYPE_ID))
        {
        case GAMEOBJECT_TYPE_TRANSPORT:
        case GAMEOBJECT_TYPE_MO_TRANSPORT:
            {
                updateFlags |= UPDATEFLAG_DYN_MODEL|UPDATEFLAG_TRANSPORT;
            }break;
        case GAMEOBJECT_TYPE_TRAP:
        case GAMEOBJECT_TYPE_DUEL_ARBITER:
        case GAMEOBJECT_TYPE_FLAGSTAND:
        case GAMEOBJECT_TYPE_FLAGDROP:
            {
                // duel flags have to stay as updatetype 3, otherwise
                // it won't animate
                updatetype = UPDATETYPE_CREATE_PLAYEROBJ;
            }break;
        }
    } else if(IsObject())
        updateFlags |= UPDATEFLAG_STATIONARY_POS;

    if(target == this)
    {
        // player creating self
        updateFlags |= UPDATEFLAG_SELF;
        updatetype = UPDATETYPE_CREATE_PLAYEROBJ;
    }

    if(IsUnit())
    {
        if (castPtr<Unit>(this)->GetUInt64Value(UNIT_FIELD_TARGET))
            updateFlags |= UPDATEFLAG_HAS_TARGET;
    }

    // build our actual update
    *data << uint8(updatetype);
    *data << m_objGuid.asPacked();
    *data << uint8(GetTypeId());
    // Send our object type update
    _BuildMovementUpdate(data, updateFlags, target);
    // this will cache automatically if needed
    _BuildCreateValuesUpdate( data, target );
    // update count: 1 ;)
    return 1;
}

uint32 Object::BuildValuesUpdateBlockForPlayer(ByteBuffer *data, uint32 updateFlags)
{
    UpdateMask updateMask(m_valuesCount);
    if(_SetUpdateBits(&updateMask, updateFlags))
    {
        *data << uint8(UPDATETYPE_VALUES);     // update type == update
        *data << m_objGuid.asPacked();
        _BuildChangedValuesUpdate( data, &updateMask );
        return 1;
    }
    return 0;
}

uint32 Object::BuildOutOfRangeUpdateBlock(ByteBuffer *data)
{
    *data << GetGUID().asPacked();
    return 1;
}

//=======================================================================================
//  Creates an update block containing all data needed for a new object
//=======================================================================================
void Object::_BuildCreateValuesUpdate(ByteBuffer * data, Player* target)
{
    ByteBuffer fields;
    UpdateMask mask(m_valuesCount);
    uint16 typeMask = GetTypeFlags(), uFlag = GetUpdateFlag(target), offset = 0, *flags, fLen = 0;
    for(uint8 f = 0; f < 10; f++)
    {
        if(typeMask & 1<<f)
        {
            GetUpdateFieldData(f, flags, fLen);
            for(uint32 i = 0; i < fLen; i++, offset++)
            {
                if(m_uint32Values[offset] == 0)
                    continue;

                if(flags[i] & (uFlag|m_notifyFlags))
                {
                    mask.SetBit(offset);
                    fields << m_uint32Values[offset];
                }
            }
        }
    }

    uint32 byteCount = mask.GetUpdateBlockCount();
    *data << uint8(byteCount);
    data->append( mask.GetMask(), byteCount*4 );
    data->append(fields.contents(), fields.size());
}

//=======================================================================================
//  Creates an update block with the values of this object as
//  determined by the updateMask.
//=======================================================================================
void Object::_BuildChangedValuesUpdate(ByteBuffer * data, UpdateMask *updateMask)
{
    WPAssert( updateMask && updateMask->GetCount() == m_valuesCount );
    uint32 byteCount = updateMask->GetUpdateBlockCount();
    uint32 valueCount = std::min(byteCount*32, m_valuesCount);

    *data << uint8(byteCount);
    data->append( updateMask->GetMask(), byteCount*4 );
    for( uint32 index = 0; index < valueCount; index++ )
        if( updateMask->GetBit( index ) )
            *data << m_uint32Values[index];
}

///////////////////////////////////////////////////////////////
/// Build the Movement Data portion of the update packet
/// Fills the data with this object's movement/speed info
void Object::_BuildMovementUpdate(ByteBuffer * data, uint16 flags, Player* target )
{
    data->WriteBit(0);
    data->WriteBit(0);
    data->WriteBit(flags & UPDATEFLAG_ROTATION);
    data->WriteBit(flags & UPDATEFLAG_ANIMKITS);
    data->WriteBit(flags & UPDATEFLAG_HAS_TARGET);
    data->WriteBit(flags & UPDATEFLAG_SELF);
    data->WriteBit(flags & UPDATEFLAG_VEHICLE);
    data->WriteBit(flags & UPDATEFLAG_LIVING);
    data->WriteBits(0, 24);
    data->WriteBit(0);
    data->WriteBit(flags & UPDATEFLAG_GO_TRANSPORT_POS);
    data->WriteBit(flags & UPDATEFLAG_STATIONARY_POS);
    data->WriteBit(flags & UPDATEFLAG_TRANSPORT_ARR);
    data->WriteBit(0);
    data->WriteBit(flags & UPDATEFLAG_TRANSPORT);

    if(flags & UPDATEFLAG_LIVING)
        _WriteLivingMovementUpdateBits(data, target);

    // used only with GO's, placeholder
    if (flags & UPDATEFLAG_GO_TRANSPORT_POS)
        data->WriteBitString(10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    if(flags & UPDATEFLAG_HAS_TARGET)
        _WriteTargetMovementUpdateBits(data, target);

    if (flags & UPDATEFLAG_ANIMKITS)
        data->WriteBitString(3, 1, 1, 1); // Missing Anim kits 1,2,3

    data->FlushBits();
    if(flags & UPDATEFLAG_LIVING)
        _WriteLivingMovementUpdateBytes(data, target);

    if(flags & UPDATEFLAG_VEHICLE)
        *data << float(castPtr<WorldObject>(this)->GetOrientation()) << castPtr<Vehicle>(this)->GetVehicleEntry();

    if (flags & UPDATEFLAG_GO_TRANSPORT_POS)
    {
        // no transguid 0, 5
        // No transtime3
        // no tansguid 3
        *data << float(0.f);
        // no transguid 4, 6, 1
        *data << uint32(0);
        *data << float(0.f);
        // no transguid 2, 7
        *data << float(0.f);
        *data << uint8(0xFF);
        *data << float(0.f);
        // No transtime2
    }

    if(flags & UPDATEFLAG_ROTATION)
    {
        uint64 rotation = 0;
        if(IsGameObject()) rotation = castPtr<GameObject>(this)->m_rotation;
        *data << uint64(rotation); //blizz 64bit rotation
    }

    if (flags & UPDATEFLAG_TRANSPORT_ARR)
    {
        for(uint8 i = 0; i < 4; i++)
            *data << float(0.0f);
        *data << uint8(0);
        for(uint8 x = 0; x < 3; x++)
            for(uint8 y = 0; y < 4; y++)
                *data << float(0.0f);
    }

    if (flags & UPDATEFLAG_STATIONARY_POS)
        _WriteStationaryPositionBytes(data, target);

    if(flags & UPDATEFLAG_HAS_TARGET)
        _WriteTargetMovementUpdateBytes(data, target);

    if (flags & UPDATEFLAG_ANIMKITS)
    {
        if(false) *data << uint16(0); // AnimKit1
        if(false) *data << uint16(0); // AnimKit2
        if(false) *data << uint16(0); // AnimKit3
    }

    if(flags & UPDATEFLAG_TRANSPORT)
        *data << uint32(getMSTime());
}

void Object::_WriteStationaryPositionBytes(ByteBuffer *bytes, Player *target)
{
    printf("Writing empty stationary\n");
    *bytes << float(0.f) << float(0.f) << float(0.f) << float(0.f);
}

void Object::_WriteLivingMovementUpdateBits(ByteBuffer *bits, Player *target)
{
    printf("Writing empty livingbit\n");
    bits->WriteBit(1); // We have no movement flags
    bits->WriteBit(1); // We have no orientation
    bits->WriteBits(0, 3); // Guid mask
    // 30 movementflag bits
    bits->WriteBit(0); // unk
    bits->WriteBit(1); // We have no pitch
    bits->WriteBit(0); // We have spline disabled
    bits->WriteBit(0); // We have no fall data
    bits->WriteBit(1); // We have no elevation
    bits->WriteBit(0); // Guid mask
    bits->WriteBit(0); // We have no transport
    bits->WriteBit(1); // We have no timestamp
    // Transport bits
    bits->WriteBit(0); // Guid mask
    // Spline bits
    bits->WriteBit(0); // Guid mask
    // hasFalldirection
    bits->WriteBit(0); // Guid mask
    bits->WriteBit(0); // Unk
    bits->WriteBit(1); // We have no movementflags2
    // Movementflag2 bits
}

void Object::_WriteTargetMovementUpdateBits(ByteBuffer *bits, Player *target)
{
    printf("Writing empty targetbit\n");
    // Objects have no targets, this will be overwritten by Unit::_WriteTargetMovementUpdate
    bits->WriteBits(0, 8);
}

void Object::_WriteLivingMovementUpdateBytes(ByteBuffer *bytes, Player *target) { printf("Writing empty livingbyte\n"); }
void Object::_WriteTargetMovementUpdateBytes(ByteBuffer *bytes, Player *target) { printf("Writing empty targetbyte\n"); }

void Object::DestroyForPlayer(Player* target, bool anim)
{
    if(target == NULL)
        return;

    WorldPacket data(SMSG_DESTROY_OBJECT, 9);
    data << GetGUID();
    data << uint8(anim ? 1 : 0);
    target->GetSession()->SendPacket( &data );
}

void Object::ClearLoot()
{
    // better cancel any rolls just in case.
    for(std::vector<__LootItem>::iterator itr = m_loot.items.begin(); itr != m_loot.items.end(); itr++)
    {
        if( itr->roll != NULL )
        {
            sEventMgr.RemoveEvents(itr->roll);
            itr->roll = NULL; // buh-bye!
        }
    }

    m_loot.gold = 0;
    m_loot.items.clear();
    m_loot.looters.clear();
}

//===============================================
// WorldObject class functions
//===============================================
WorldObject::WorldObject(uint64 guid, uint32 fieldCount) : Object(guid, fieldCount), m_position(0,0,0,0)
{
    m_mapId = -1;
    m_areaId = 0;
    m_zoneId = 0;
    m_areaFlags = 0;
    m_lastMovementZone = 0;

    m_mapMgr = NULL;
    m_mapCell = 0;

    m_factionTemplate = NULL;

    m_instanceId = 0;
    Active = false;
}

WorldObject::~WorldObject( )
{

}

void WorldObject::Init()
{

}

void WorldObject::Destruct()
{
    if(IsInWorld())
        RemoveFromWorld(false);

    if(GetMapCell())
    {
        // Remove object from cell
        GetMapCell()->RemoveObject(this);

        // Unset object's cell
        SetMapCell(NULL);
    }

    ClearInRangeSet();

    // for linux
    m_instanceId = -1;
    sEventMgr.RemoveEvents(this);
    Object::Destruct();
}

void WorldObject::_WriteStationaryPositionBytes(ByteBuffer *bytes, Player *target)
{
    *bytes << GetOrientation() << GetPositionX() << GetPositionY() << GetPositionZ();
}

//That is dirty fix it actually creates update of 1 field with
//the given value ignoring existing changes in fields and so on
//usefull if we want update this field for certain players
//NOTE: it does not change fields. This is also very fast method
WorldPacket *WorldObject::BuildFieldUpdatePacket(uint32 index, uint32 value)
{
    WorldPacket * packet = new WorldPacket(SMSG_UPDATE_OBJECT, 1500);
    *packet << uint16(GetMapId());
    *packet << uint32(1);//number of update/create blocks
    BuildFieldUpdatePacket(packet, index, value);
    return packet;
}

void WorldObject::BuildFieldUpdatePacket(Player* Target, uint32 Index, uint32 Value)
{
    ByteBuffer buf(500);
    BuildFieldUpdatePacket(&buf, Index, Value);
    Target->PushUpdateBlock(&buf, 1);
}

void WorldObject::BuildFieldUpdatePacket(ByteBuffer * buf, uint32 Index, uint32 Value)
{
    *buf << uint8(UPDATETYPE_VALUES);
    *buf << m_objGuid.asPacked();

    uint32 mBlocks = Index/32+1;
    *buf << (uint8)mBlocks;

    for(uint32 dword_n = mBlocks-1; dword_n; dword_n--)
        *buf <<(uint32)0;

    *buf <<(((uint32)(1))<<(Index%32));
    *buf << Value;
}

/* Crow:
    This function is a really heavy height check that is used for getting all types of height checks!
    Since it is in the object class, we can skip any actual variables and instead use our current position.
    This function gets map height and also checks the position of WMO's so that the height is set to the WMO height
    based on the Z position that is given. If no Z position is given, but x and y positions are given, we will instead
    use basic map heights as our Z position. */
float WorldObject::GetCHeightForPosition(bool checkwater, float x, float y, float z)
{
    if(!IsInWorld())
        return 0.0f;

    MapMgr* mgr = GetMapMgr();
    float offset = 0.12156f;
    if(x == 0.0f && y == 0.0f)
    {
        x = GetPositionX();
        y = GetPositionY();
    }

    if(z == 0.0f)
        z = GetPositionZ();

    if(!mgr->GetBaseMap() || !mgr->GetBaseMap()->GetMapTerrain())
        return z;

    float waterheight = mgr->GetWaterHeight(x, y, z);
    float mapheight = mgr->GetLandHeight(x, y);
    if(!mgr->CanUseCollision(this))
    {
        if(checkwater && waterheight != NO_WATER_HEIGHT)
            if(waterheight > mapheight)
                return waterheight+offset;
        return mapheight+offset;
    }

    float vmapheight = sVMapInterface.GetHeight(GetMapId(), GetInstanceID(), 0, x, y, z);
    if(IS_INSTANCE(mgr->GetMapId()) || !sWorld.CalculatedHeightChecks)
    {
        if(vmapheight != NO_WMO_HEIGHT)
        {
            if(checkwater && waterheight != NO_WATER_HEIGHT)
                if(waterheight > vmapheight)
                    return waterheight+offset;
            return vmapheight+offset;
        }

        if(checkwater && waterheight != NO_WATER_HEIGHT)
            if(waterheight > mapheight)
                return waterheight+offset;
        return mapheight+offset;
    }

    float phx = 0.0f;
    float phy = 0.0f;
    float phz = 0.0f;
    float CMapHeight = NO_LAND_HEIGHT;
    sVMapInterface.GetFirstPoint(GetMapId(), GetInstanceID(), GetPhaseMask(), x, y, z-10.0f, x, y, z+10.0f, phx, phy, CMapHeight, 0.0f);

    // Mapheight first.
    if(mapheight != NO_LAND_HEIGHT)
    {
        if(mapheight-2.0f < z)
        {
            if(mapheight+2.0f > z) // Accurate map height
            {
                if(checkwater && waterheight != NO_WATER_HEIGHT)
                    if(waterheight > mapheight)
                        return waterheight+offset;
                return mapheight+offset;
            }

            if(!sVMapInterface.GetFirstPoint(GetMapId(), GetInstanceID(), GetPhaseMask(), x, y, mapheight, x, y, z, phx, phy, phz, 0.0f))
            {
                if(checkwater && waterheight != NO_WATER_HEIGHT)
                    if(waterheight > mapheight)
                        return waterheight+offset;
                return mapheight+offset; // No breaks inbetween us, so its the right height, we're just a bunch of fuckers!
            }

            // TODO
        }
        else if(mapheight-10.0f > z)
        {
            // TODO
        }
    }

    // Now Vmap Height
    if(vmapheight != NO_WMO_HEIGHT)
    {
        if(vmapheight-2.0f < z)
        {
            if(vmapheight+2.0f > z) // Accurate map height
            {
                if(checkwater && waterheight != NO_WATER_HEIGHT)
                    if(waterheight > vmapheight)
                        return waterheight+offset;
                return vmapheight+offset;
            }

            if(!sVMapInterface.GetFirstPoint(GetMapId(), GetInstanceID(), GetPhaseMask(), x, y, vmapheight, x, y, z, phx, phy, phz, 0.0f))
            {
                if(checkwater && waterheight != NO_WATER_HEIGHT)
                    if(waterheight > vmapheight)
                        return waterheight+offset;
                return vmapheight+offset; // No breaks inbetween us, so its the right height, we're just a bunch of fuckers!
            }

            if(phz > z)
            {
                if(vmapheight < z)
                {
                    float mmapheight = NavMeshInterface.GetWalkingHeight(GetMapId(), x, y, z, vmapheight);
                    if(mmapheight != MMAP_UNAVAILABLE)
                    {
                        if(checkwater && waterheight != NO_WATER_HEIGHT)
                            if(waterheight > mmapheight)
                                return waterheight+offset;
                        return mmapheight+offset;
                    }
                }
            }
            else
            {
                float mmapheight = NavMeshInterface.GetWalkingHeight(GetMapId(), x, y, z, phz);
                if(mmapheight != MMAP_UNAVAILABLE)
                {
                    if(checkwater && waterheight != NO_WATER_HEIGHT)
                        if(waterheight > mmapheight)
                            return waterheight+offset;
                    return mmapheight+offset;
                }
            }
        }
        else
        {
            float mmapheight = NavMeshInterface.GetWalkingHeight(GetMapId(), x, y, z, vmapheight);
            if(mmapheight != MMAP_UNAVAILABLE)
            {
                if(checkwater && waterheight != NO_WATER_HEIGHT)
                    if(waterheight > mmapheight)
                        return waterheight+offset;
                return mmapheight+offset;
            }
        }
    }

    // I think it's safe to say, no one is ever perfect.
    if((CMapHeight != z+10.0f) && (CMapHeight != z-10.0f))
    {
        if(checkwater && waterheight != NO_WATER_HEIGHT)
            if(waterheight > CMapHeight)
                return waterheight+offset;
        return CMapHeight+offset;
    }

    if(checkwater && waterheight != NO_WATER_HEIGHT)
        return waterheight;
    return z;
}

void WorldObject::_Create( uint32 mapid, float x, float y, float z, float ang )
{
    m_mapId = mapid;
    m_position.ChangeCoords(x, y, z, ang);
}

WorldPacket * WorldObject::BuildTeleportAckMsg(const LocationVector & v)
{
    ///////////////////////////////////////
    //Update player on the client with TELEPORT_ACK
    if( IsInWorld() )       // only send when inworld
        castPtr<Player>(this)->SetPlayerStatus( TRANSFER_PENDING );

    WorldPacket * data = new WorldPacket(MSG_MOVE_TELEPORT, 80);
    data->WriteBitString(10, m_objGuid[6], m_objGuid[0], m_objGuid[3], m_objGuid[2], 0, 0, m_objGuid[1], m_objGuid[4], m_objGuid[7], m_objGuid[5]);
    data->FlushBits();
    *data << uint32(0);
    data->WriteByteSeq(m_objGuid[1]);
    data->WriteByteSeq(m_objGuid[2]);
    data->WriteByteSeq(m_objGuid[3]);
    data->WriteByteSeq(m_objGuid[5]);
    *data << float(v.x);
    data->WriteByteSeq(m_objGuid[4]);
    *data << float(v.o);
    data->WriteByteSeq(m_objGuid[7]);
    *data << float(v.z);
    data->WriteByteSeq(m_objGuid[0]);
    data->WriteByteSeq(m_objGuid[6]);
    *data << float(v.y);
    return data;
}

void WorldObject::OnFieldUpdated(uint32 index)
{
    if(IsInWorld() && !m_objectUpdated)
    {
        m_mapMgr->ObjectUpdated(this);
        m_objectUpdated = true;
    }

    Object::OnFieldUpdated(index);
}

void WorldObject::SetPosition( float newX, float newY, float newZ, float newOrientation )
{
    bool updateMap = false;
    if(m_lastMapUpdatePosition.Distance2DSq(newX, newY) > 4.0f)     /* 2.0f */
        updateMap = true;

    m_position.ChangeCoords(newX, newY, newZ, newOrientation);
    if (IsInWorld() && updateMap)
    {
        m_lastMapUpdatePosition.ChangeCoords(newX,newY,newZ,newOrientation);
        m_mapMgr->ChangeObjectLocation(this);

        if( IsPlayer() && castPtr<Player>(this)->GetGroup() && castPtr<Player>(this)->m_last_group_position.Distance2DSq(m_position) > 25.0f ) // distance of 5.0
            castPtr<Player>(this)->GetGroup()->HandlePartialChange( PARTY_UPDATE_FLAG_POSITION, castPtr<Player>(this) );
    }
}

void WorldObject::DestroyForInrange(bool anim)
{
    WorldPacket data(SMSG_DESTROY_OBJECT, 9);
    data << GetGUID();
    data << uint8(anim ? 1 : 0);
    SendMessageToSet(&data, false);
}

void WorldObject::OutPacketToSet(uint16 Opcode, uint16 Len, const void * Data, bool self)
{
    if(self && GetTypeId() == TYPEID_PLAYER)
        castPtr<Player>(this)->GetSession()->OutPacket(Opcode, Len, Data);

    if(!IsInWorld())
        return;

    bool gm = GetTypeId() == TYPEID_PLAYER ? castPtr<Player>(this)->m_isGmInvisible : false;
    for(InRangeSet::iterator itr = GetInRangePlayerSetBegin(); itr != GetInRangePlayerSetEnd(); itr++)
    {
        if(Player *plr = GetInRangeObject<Player>(*itr))
        {
            if(plr->GetSession() == NULL)
                continue;
            if(gm && plr->GetSession()->GetPermissionCount() == 0)
                continue;
            plr->GetSession()->OutPacket(Opcode, Len, Data);
        }
    }
}

void WorldObject::SendMessageToSet(WorldPacket *data, bool bToSelf, bool myteam_only)
{
    if(!IsInWorld())
        return;

    uint32 myTeam = 0;
    if(IsPlayer())
    {
        if(bToSelf) castPtr<Player>(this)->GetSession()->SendPacket(data);
        myTeam = castPtr<Player>(this)->GetTeam();
    }

    bool gm = data->GetOpcode() == SMSG_MESSAGECHAT ? false : ( GetTypeId() == TYPEID_PLAYER ? castPtr<Player>(this)->m_isGmInvisible : false );
    for(InRangeSet::iterator itr = GetInRangePlayerSetBegin(); itr != GetInRangePlayerSetEnd(); itr++)
    {
        if(Player *plr = GetInRangeObject<Player>(*itr))
        {
            if(plr->GetSession() == NULL)
                continue;
            if(myteam_only && plr->GetTeam() != myTeam)
                continue;
            if(gm && plr->GetSession()->GetPermissionCount() == 0)
                continue;
            plr->GetSession()->SendPacket(data);
        }
    }
}

void WorldObject::AddToWorld()
{
    MapMgr* mapMgr = sInstanceMgr.GetInstance(this);
    if(mapMgr == NULL)
        return;

    if(IsPlayer())
    {
        // battleground checks
        Player* p = castPtr<Player>(this);
        if( p->m_bg == NULL && mapMgr->m_battleground != NULL )
        {
            // player hasn't been registered in the battleground, ok.
            // that means we re-logged into one. if it's an arena, don't allow it!
            // also, don't allow them in if the bg is full.

            if( !mapMgr->m_battleground->CanPlayerJoin(p) && !p->bGMTagOn)
                return;
        }

        // players who's group disbanded cannot remain in a raid instances alone(no soloing them:P)
        if( !p->triggerpass_cheat && p->GetGroup()== NULL && (mapMgr->GetdbcMap()->IsRaid() || mapMgr->GetMapInfo()->type == INSTANCE_MULTIMODE))
            return;
    } else UpdateAreaInfo(mapMgr);

    m_mapMgr = mapMgr;

    mapMgr->AddObject(this);

    // correct incorrect instance id's
    m_instanceId = m_mapMgr->GetInstanceID();

    Object::AddToWorld();
}

//Unlike addtoworld it pushes it directly ignoring add pool
//this can only be called from the thread of mapmgr!!!
void WorldObject::PushToWorld(MapMgr* mgr)
{
    ASSERT(mgr != NULL);
    if(mgr == NULL)
    {
        // Reset these so session will get updated properly.
        if(IsPlayer())
        {
            sLog.Error("WorldObject","Kicking Player %s due to empty MapMgr;",castPtr<Player>(this)->GetName());
            castPtr<Player>(this)->GetSession()->LogoutPlayer(false);
        }
        return; //instance add failed
    }

    m_mapId = mgr->GetMapId();
    m_instanceId = mgr->GetInstanceID();

    if(!IsPlayer())
        UpdateAreaInfo(mgr);
    m_mapMgr = mgr;
    OnPrePushToWorld();

    mgr->PushObject(this);

    // correct incorrect instance id's
    event_Relocate();

    // call virtual function to handle stuff.. :P
    OnPushToWorld();
}

void WorldObject::RemoveFromWorld(bool free_guid)
{
    // clear loot
    ClearLoot();

    ASSERT(m_mapMgr);
    MapMgr* m = m_mapMgr;
    m_mapMgr = NULL;

    m->RemoveObject(this, free_guid);

    // remove any spells / free memory
    sEventMgr.RemoveEvents(this, EVENT_UNIT_SPELL_HIT);

    // update our event holder
    event_Relocate();
    Object::RemoveFromWorld(free_guid);
}

bool WorldObject::canWalk()
{
    if(IsCreature())
    {
        Creature* ctr = castPtr<Creature>(this);
        if(ctr->GetCanMove() & LIMIT_ANYWHERE)
            return true;
        if(ctr->GetCanMove() & LIMIT_GROUND)
            return true;
    } else if(IsPlayer())
        return true;
    return false;
}

bool WorldObject::canSwim()
{
    if(IsCreature())
    {
        Creature* ctr = castPtr<Creature>(this);
        if(ctr->GetCanMove() & LIMIT_ANYWHERE)
            return true;
        if(ctr->GetCanMove() & LIMIT_WATER)
            return true;
    } else if(IsPlayer())
        return true;
    return false;
}

bool WorldObject::canFly()
{
    if(IsVehicle())
        return false;
    else if(IsCreature())
    {
        Creature* ctr = castPtr<Creature>(this);
        if(ctr->GetCanMove() & LIMIT_ANYWHERE)
            return true;
        if(ctr->GetCanMove() & LIMIT_AIR)
            return true;
    }
    else if(IsPlayer())
    {
        Player* plr = castPtr<Player>(this);
        if(plr->m_FlyingAura)
            return true;
        if(plr->FlyCheat)
            return true;
    }

    return false;
}

bool WorldObject::IsInBox(float centerX, float centerY, float centerZ, float BLength, float BWidth, float BHeight, float BOrientation, float delta)
{
    double rotation = 2*M_PI-BOrientation;
    double sinVal = sin(rotation);
    double cosVal = cos(rotation);
    float playerBoxDistX = GetPositionX() - centerX;
    float playerBoxDistY = GetPositionY() - centerY;
    float rotationPlayerX = (float)(centerX + playerBoxDistX * cosVal - playerBoxDistY*sinVal);
    float rotationPlayerY = (float)(centerY + playerBoxDistY * cosVal + playerBoxDistX*sinVal);
    float dx = rotationPlayerX - centerX;
    float dy = rotationPlayerY - centerY;
    float dz = GetPositionZ() - centerZ;
    if(!((fabs(dx) > BLength/2 + delta) || (fabs(dy) > BWidth/2 + delta) || (fabs(dz) > BHeight/2 + delta)))
        return true;
    return false;
}

////////////////////////////////////////////////////////////

float WorldObject::CalcDistance(WorldObject* Ob)
{
    return CalcDistance(GetPositionX(), GetPositionY(), GetPositionZ(), Ob->GetPositionX(), Ob->GetPositionY(), Ob->GetPositionZ());
}

float WorldObject::CalcDistance(float ObX, float ObY, float ObZ)
{
    return CalcDistance(GetPositionX(), GetPositionY(), GetPositionZ(), ObX, ObY, ObZ);
}

float WorldObject::CalcDistance(WorldObject* Oa, WorldObject* Ob)
{
    return CalcDistance(Oa->GetPositionX(), Oa->GetPositionY(), Oa->GetPositionZ(), Ob->GetPositionX(), Ob->GetPositionY(), Ob->GetPositionZ());
}

float WorldObject::CalcDistance(WorldObject* Oa, float ObX, float ObY, float ObZ)
{
    return CalcDistance(Oa->GetPositionX(), Oa->GetPositionY(), Oa->GetPositionZ(), ObX, ObY, ObZ);
}

float WorldObject::CalcDistance(float OaX, float OaY, float OaZ, float ObX, float ObY, float ObZ)
{
    float xdest = fabs(ObX - OaX);
    float ydest = fabs(ObY - OaY);
    float zdest = fabs(ObZ - OaZ);
    return sqrtf((zdest*zdest) + (ydest*ydest) + (xdest*xdest));
}

float WorldObject::calcAngle( float Position1X, float Position1Y, float Position2X, float Position2Y )
{
    float dx = Position2X-Position1X;
    float dy = Position2Y-Position1Y;
    double angle=0.0f;

    // Calculate angle
    if (dx == 0.0)
    {
        if (dy == 0.0)
            angle = 0.0;
        else if (dy > 0.0)
            angle = M_PI * 0.5 /* / 2 */;
        else
            angle = M_PI * 3.0 * 0.5/* / 2 */;
    }
    else if (dy == 0.0)
    {
        if (dx > 0.0)
            angle = 0.0;
        else
            angle = M_PI;
    }
    else
    {
        if (dx < 0.0)
            angle = atanf(dy/dx) + M_PI;
        else if (dy < 0.0)
            angle = atanf(dy/dx) + (2*M_PI);
        else
            angle = atanf(dy/dx);
    }

    // Convert to degrees
    angle = angle * float(180 / M_PI);

    // Return
    return float(angle);
}

float WorldObject::calcRadAngle( float Position1X, float Position1Y, float Position2X, float Position2Y )
{
    double dx = double(Position2X-Position1X);
    double dy = double(Position2Y-Position1Y);
    double angle=0.0;

    // Calculate angle
    if (dx == 0.0)
    {
        if (dy == 0.0)
            angle = 0.0;
        else if (dy > 0.0)
            angle = M_PI * 0.5/*/ 2.0*/;
        else
            angle = M_PI * 3.0 * 0.5/*/ 2.0*/;
    }
    else if (dy == 0.0)
    {
        if (dx > 0.0)
            angle = 0.0;
        else
            angle = M_PI;
    }
    else
    {
        if (dx < 0.0)
            angle = atan(dy/dx) + M_PI;
        else if (dy < 0.0)
            angle = atan(dy/dx) + (2*M_PI);
        else
            angle = atan(dy/dx);
    }

    // Return
    return float(angle);
}

float WorldObject::getEasyAngle( float angle )
{
    while ( angle < 0 ) {
        angle = angle + 360;
    }
    while ( angle >= 360 ) {
        angle = angle - 360;
    }
    return angle;
}

bool WorldObject::inArc(float Position1X, float Position1Y, float FOV, float Orientation, float Position2X, float Position2Y )
{
    float angle = calcAngle( Position1X, Position1Y, Position2X, Position2Y );
    float lborder = getEasyAngle( ( Orientation - (FOV*0.5f/*/2*/) ) );
    float rborder = getEasyAngle( ( Orientation + (FOV*0.5f/*/2*/) ) );
    //sLog.outDebug("Orientation: %f Angle: %f LeftBorder: %f RightBorder %f",Orientation,angle,lborder,rborder);
    if(((angle >= lborder) && (angle <= rborder)) || ((lborder > rborder) && ((angle < rborder) || (angle > lborder))))
        return true;
    return false;
}

// Return angle in range 0..2*pi
float GetAngle(float x, float y, float Targetx, float Targety)
{
    float dx = Targetx - x;
    float dy = Targety - y;

    float ang = atan2(dy, dx);
    ang = (ang >= 0) ? ang : 2 * M_PI + ang;
    return ang;
}

float Normalize(float o)
{
    if (o < 0)
    {
        float mod = o *-1;
        mod = fmod(mod, 2.0f * static_cast<float>(M_PI));
        mod = -mod + 2.0f * static_cast<float>(M_PI);
        return mod;
    }
    return fmod(o, 2.0f * static_cast<float>(M_PI));
}

bool WorldObject::isTargetInFront(WorldObject* target)
{
    // always have self in arc
    if (target == this)
        return true;

    // move arc to range 0.. 2*pi
    float arc = Normalize(static_cast<float>(M_PI/2));

    float angle = GetAngle(GetPositionX(), GetPositionY(), target->GetPositionX(), target->GetPositionY());
    angle -= m_position.o;

    // move angle to range -pi +pi
    angle = Normalize(angle);
    if(angle > M_PI)
        angle -= 2.0f*M_PI;

    float lborder =  -1 * (arc/2.0f);               // in range -pi..0
    float rborder = (arc/2.0f);                     // in range 0..pi
    return ((angle >= lborder) && (angle <= rborder));
}

bool WorldObject::isInArc(WorldObject* target , float angle) // angle in degrees
{
    return inArc( GetPositionX() , GetPositionY() , angle , GetOrientation() , target->GetPositionX() , target->GetPositionY() );
}

bool WorldObject::isInRange(WorldObject* target, float range)
{
    float dist = CalcDistance( target );
    return( dist <= range );
}

void WorldObject::_setFaction()
{
    // Clear our old faction info
    if(GetTypeId() == TYPEID_UNIT || IsPlayer())
        m_factionTemplate = dbcFactionTemplate.LookupEntry(GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE));
    else if(GetTypeId() == TYPEID_GAMEOBJECT)
        m_factionTemplate = dbcFactionTemplate.LookupEntry(GetUInt32Value(GAMEOBJECT_FACTION));
}

int32 WorldObject::DealDamage(Unit* pVictim, uint32 damage, uint32 targetEvent, uint32 unitEvent, uint32 spellId, bool no_remove_auras)
{
    if(!IsInWorld())
        return 0;
    if( !pVictim || !pVictim->isAlive() || !pVictim->IsInWorld())
        return 0;
    if( pVictim->bInvincible == true )
        return 0;
    if( pVictim->IsSpiritHealer() )
        return 0;

    if( pVictim->GetStandState() )//not standing-> standup
        pVictim->SetStandState( STANDSTATE_STAND );//probably mobs also must standup

    Player* plr = NULL;
    if(IsPet())
        plr = castPtr<Pet>(this)->GetPetOwner();
    else if(IsPlayer())
        plr = castPtr<Player>(this);

    // Player we are attacking, or the owner of totem/pet/etc
    Player *pOwner = pVictim->IsPlayer() ? castPtr<Player>(pVictim) : NULL;

    // This is the player or the player controlling the totem/pet/summon
    Player *pAttacker = IsPlayer() ? castPtr<Player>(this) : NULL;

    // We identified both the attacker and the victim as possible PvP combatants, if we are not dueling we will flag the attacker
    if( pOwner != NULL && pAttacker != NULL && pOwner != pAttacker && pOwner != pAttacker->DuelingWith )
    {
        if( !pAttacker->IsPvPFlagged() )
        {
            pAttacker->PvPToggle();
        }
    }

    // PvP NPCs will flag the player when attacking them
    if( pVictim->IsCreature() && pVictim->IsPvPFlagged() && pAttacker != NULL )
    {
        if( !pAttacker->IsPvPFlagged() )
        {
            pAttacker->PvPToggle();
        }
    }

    //If our pet attacks  - flag us.
    if( pVictim->IsPlayer() && IsPet() )
    {
        Player* owner = castPtr<Player>( castPtr<Pet>(this)->GetPetOwner() );
        if( owner != NULL )
            if( owner->isAlive() && castPtr<Player>( pVictim )->DuelingWith != owner )
                owner->SetPvPFlag();
    }

    if(!no_remove_auras)
    {
        float breakchance = 35.0f;
        if( spellId == 51514)// && IsUnit() && HasDummyAura(SPELL_HASH_GLYPH_OF_HEX) ) wait till 3.1
            breakchance += 15.0f;

        //zack 2007 04 24 : root should not remove self (and also other unknown spells)
        if(spellId)
            pVictim->m_AuraInterface.RemoveAllAurasByInterruptFlagButSkip(AURA_INTERRUPT_ON_ANY_DAMAGE_TAKEN, spellId);
        else
            pVictim->m_AuraInterface.RemoveAllAurasByInterruptFlag(AURA_INTERRUPT_ON_ANY_DAMAGE_TAKEN);
    }

    if(IsUnit() && castPtr<Unit>(this)->isAlive() )
    {
        if( castPtr<Unit>(this) != pVictim && pVictim->IsPlayer() && IsPlayer() && castPtr<Player>(this)->m_hasInRangeGuards )
        {
            castPtr<Player>(this)->SetGuardHostileFlag(true);
            castPtr<Player>(this)->CreateResetGuardHostileFlagEvent();
        }

        if(plr != NULL && pVictim->IsCreature())
            castPtr<Creature>(pVictim)->Tag(plr);

        // Pepsi1x1: is this correct this
        if( pVictim != castPtr<Unit>(this))
            castPtr<Unit>(this)->CombatStatus.OnDamageDealt( pVictim, damage );
    }

    ///Rage
    if( pVictim->GetMaxPower(POWER_TYPE_RAGE) > 0
        && pVictim != castPtr<Unit>(this)
        && pVictim->IsPlayer())
    {
        float level = (float)pVictim->getLevel();
        float c = 0.0091107836f * level * level + 3.225598133f * level + 4.2652911f;
        uint32 rage = pVictim->GetPower(POWER_TYPE_RAGE);
        float val = 2.5f * damage / c;
        rage += float2int32(val) * 10;
        if( rage > pVictim->GetMaxPower(POWER_TYPE_RAGE) )
            rage = pVictim->GetMaxPower(POWER_TYPE_RAGE);

        pVictim->SetPower(POWER_TYPE_RAGE, rage);
        pVictim->SendPowerUpdate();
    }

    //* BATTLEGROUND DAMAGE COUNTER *//
    if( pVictim != castPtr<Unit>(this) && plr != NULL )
    {
        if(plr->m_bg != NULL)
        {
            plr->m_bgScore.DamageDone += damage;
            plr->m_bg->UpdatePvPData();
        }
    }

    uint32 health = pVictim->GetUInt32Value(UNIT_FIELD_HEALTH );

    /*------------------------------------ DUEL HANDLERS --------------------------*/
    if(pVictim->IsPlayer() && castPtr<Player>(pVictim)->DuelingWith != NULL) //Both Players
    {
        if(health <= damage)
        {
            if(IsPlayer() && (castPtr<Player>(pVictim)->DuelingWith == castPtr<Player>(this)))
            {
                // End Duel
                castPtr<Player>(this)->EndDuel(DUEL_WINNER_KNOCKOUT);

                // surrender emote
                castPtr<Player>(pVictim)->Emote(EMOTE_ONESHOT_BEG);           // Animation

                // Remove Negative Auras from duelist.
                castPtr<Player>(pVictim)->m_AuraInterface.RemoveAllNegAurasFromGUID(GetGUID());

                // Player in Duel and Player Victim has lost
                castPtr<Player>(pVictim)->CombatStatus.Vanish(GetLowGUID());
                castPtr<Player>(this)->CombatStatus.Vanish(pVictim->GetLowGUID());

                damage = health-5;
            } else if(castPtr<Player>(pVictim)->DuelingWith != NULL)
                castPtr<Player>(pVictim)->DuelingWith->EndDuel(DUEL_WINNER_KNOCKOUT);
        }
    }

    if((pVictim->IsPlayer()) && (IsPet()))
    {
        if((health <= damage) && castPtr<Player>(pVictim)->DuelingWith == castPtr<Pet>(this)->GetPetOwner())
        {
            Player* petOwner = castPtr<Pet>(this)->GetPetOwner();
            if(petOwner)
            {
                //Player in Duel and Player Victim has lost
                uint32 NewHP = pVictim->GetUInt32Value(UNIT_FIELD_MAXHEALTH)/100;
                if(NewHP < 5)
                    NewHP = 5;

                //Set there health to 1% or 5 if 1% is lower then 5
                pVictim->SetUInt32Value(UNIT_FIELD_HEALTH, NewHP);
                //End Duel
                petOwner->EndDuel(DUEL_WINNER_KNOCKOUT);
                return health-5;
            }
        }
    }

    /*------------------------------------ DUEL HANDLERS END--------------------------*/

    bool isCritter = false;
    if(pVictim->GetTypeId() == TYPEID_UNIT && castPtr<Creature>(pVictim)->GetCreatureData())
    {
        if(castPtr<Creature>(pVictim)->GetCreatureData()->Type == CRITTER)
            isCritter = true;
        else if(isTargetDummy(castPtr<Creature>(pVictim)->GetCreatureData()->Entry) && health <= damage)
        {   //Dummy trainers can't die
            uint32 newh = 5; // Just limit to 5HP (can't use 1HP here).
            if(pVictim->GetMaxHealth() < 5)
                newh = pVictim->GetMaxHealth();

            pVictim->SetUInt32Value(UNIT_FIELD_HEALTH, newh);
            return health-5;
        }
    }

    /* -------------------------- HIT THAT CAUSES VICTIM TO DIE ---------------------------*/
    if ((isCritter || health <= damage) )
    {
        if( pVictim->HasDummyAura(SPELL_HASH_GUARDIAN_SPIRIT) )
        {
            pVictim->CastSpell(pVictim, dbcSpell.LookupEntry(48153), true);
            pVictim->RemoveDummyAura(SPELL_HASH_GUARDIAN_SPIRIT);
            return 0;
        }

        //warlock - seed of corruption
        if( IsUnit() )
        {
            if( IsPlayer() && pVictim->IsUnit() && !pVictim->IsPlayer() && m_mapMgr->m_battleground && m_mapMgr->m_battleground->GetType() == BATTLEGROUND_ALTERAC_VALLEY )
                castPtr<AlteracValley>(m_mapMgr->m_battleground)->HookOnUnitKill( castPtr<Player>(this), pVictim );
        }

        // check if pets owner is combat participant
        bool owner_participe = false;
        if( IsPet() )
        {
            Player* owner = castPtr<Pet>(this)->GetPetOwner();
            if( owner != NULL && pVictim->GetAIInterface()->getThreat( owner->GetGUID() ) > 0 )
                owner_participe = true;
        }

        /* victim died! */
        Unit* pKiller = pVictim->CombatStatus.GetKiller();
        if( pVictim->IsPlayer() )
        {
            // let's see if we have shadow of death
            if( !pVictim->m_AuraInterface.FindPositiveAuraByNameHash(SPELL_HASH_SHADOW_OF_DEATH) && castPtr<Player>( pVictim)->HasSpell( 49157 )  &&
                !(castPtr<Player>(pVictim)->m_bg && castPtr<Player>(pVictim)->m_bg->IsArena())) //check for shadow of death
            {
                SpellEntry* sorInfo = dbcSpell.LookupEntry(54223);
                if( sorInfo != NULL && castPtr<Player>(pVictim)->Cooldown_CanCast( sorInfo ))
                {
                    SpellCastTargets targets(pVictim->GetGUID());
                    if(Spell* sor = new Spell( pVictim, sorInfo))
                        sor->prepare(&targets, false);
                    return 0;
                }
            }

            castPtr<Player>( pVictim )->KillPlayer();

            /* Remove all Auras */
            pVictim->m_AuraInterface.EventDeathAuraRemoval();

            /* Set victim health to 0 */
            pVictim->SetUInt32Value(UNIT_FIELD_HEALTH, 0);
            TRIGGER_INSTANCE_EVENT( m_mapMgr, OnPlayerDeath )( castPtr<Player>(pVictim), pKiller );
        }
        else
        {
            pVictim->SetDeathState( JUST_DIED );
            /* Remove all Auras */
            pVictim->m_AuraInterface.EventDeathAuraRemoval();
            /* Set victim health to 0 */
            pVictim->SetUInt32Value(UNIT_FIELD_HEALTH, 0);

            TRIGGER_INSTANCE_EVENT( m_mapMgr, OnCreatureDeath )( castPtr<Creature>(pVictim), pKiller );
        }

        pVictim->SummonExpireAll(false);

        if( pVictim->IsPlayer() && (!IsPlayer() || pVictim == castPtr<Unit>(this) ) )
            castPtr<Player>( pVictim )->DeathDurabilityLoss(0.10);

        /* Zone Under Attack */
        MapInfo * pZMapInfo = LimitedMapInfoStorage.LookupEntry(GetMapId());
        if( pZMapInfo != NULL && pZMapInfo->type == INSTANCE_NULL && !pVictim->IsPlayer() && !pVictim->IsPet() && ( IsPlayer() || IsPet() ) )
        {
            // Only NPCs that bear the PvP flag can be truly representing their faction.
            if( castPtr<Creature>(pVictim)->IsPvPFlagged() )
            {
                Player* pAttacker = NULL;
                if( IsPet() )
                    pAttacker = castPtr<Pet>(this)->GetPetOwner();
                else if(IsPlayer())
                    pAttacker = castPtr<Player>(this);

                if( pAttacker != NULL)
                {
                    uint8 teamId = (uint8)pAttacker->GetTeam();
                    if(teamId == 0) // Swap it.
                        teamId = 1;
                    else
                        teamId = 0;
                    uint32 AreaID = pVictim->GetAreaId();
                    if(AreaID)
                    {
                        WorldPacket data(SMSG_ZONE_UNDER_ATTACK, 4);
                        data << AreaID;
                        sWorld.SendFactionMessage(&data, teamId);
                    }
                }
            }
        }

        if(pVictim->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT) > 0)
        {
            if(pVictim->GetCurrentSpell())
            {
                Spell* spl = pVictim->GetCurrentSpell();
                for(int i = 0; i < 3; i++)
                {
                    if(spl->GetSpellProto()->Effect[i] == SPELL_EFFECT_PERSISTENT_AREA_AURA)
                    {
                        DynamicObject* dObj = GetMapMgr()->GetDynamicObject(pVictim->GetUInt32Value(UNIT_FIELD_CHANNEL_OBJECT));
                        if(dObj != NULL)
                        {
                            WorldPacket data(SMSG_GAMEOBJECT_DESPAWN_ANIM, 8);
                            data << dObj->GetGUID();
                            dObj->SendMessageToSet(&data, false);
                            dObj->RemoveFromWorld(true);
                            dObj->Destruct();
                            dObj = NULL;
                        }
                    }
                }
                if(spl->GetSpellProto()->ChannelInterruptFlags == 48140)
                    spl->cancel();
            }
        }

        if(pVictim->IsPlayer())
        {
            uint32 self_res_spell = 0;
            Player* plrVictim = castPtr<Player>(pVictim);
            if(!(plrVictim->m_bg && plrVictim->m_bg->IsArena())) // Can't self res in Arena
            {
                self_res_spell = plrVictim->SoulStone;
                plrVictim->SoulStone = plrVictim->SoulStoneReceiver = 0;

                if( !self_res_spell && plrVictim->bReincarnation )
                {
                    SpellEntry* m_reincarnSpellInfo = dbcSpell.LookupEntry( 20608 );
                    if( plrVictim->Cooldown_CanCast( m_reincarnSpellInfo ) )
                    {
                        uint32 ankh_count = plrVictim->GetInventory()->GetItemCount( 17030 );
                        if( ankh_count || castPtr<Player>(plrVictim)->HasDummyAura(SPELL_HASH_GLYPH_OF_RENEWED_LIFE ))
                            self_res_spell = 21169;
                    }
                }
            }

            pVictim->SetUInt32Value( PLAYER_SELF_RES_SPELL, self_res_spell );
            pVictim->Dismount();
        }

        // Wipe our attacker set on death
        pVictim->CombatStatus.Vanished();

        /* Stop Units from attacking */
        if( pAttacker && pAttacker->IsInWorld() )
            pAttacker->EventAttackStop();

        if( IsUnit() )
        {
            castPtr<Unit>(this)->smsg_AttackStop( pVictim );

            /* Tell Unit that it's target has Died */
            castPtr<Unit>(this)->addStateFlag( UF_TARGET_DIED );
        }

        if( pVictim->IsPlayer() )
        {
            if( castPtr<Player>( pVictim)->HasDummyAura(SPELL_HASH_SPIRIT_OF_REDEMPTION) ) //check for spirit of Redemption
            {
                if (SpellEntry* sorInfo = dbcSpell.LookupEntry(27827))
                {
                    pVictim->SetUInt32Value(UNIT_FIELD_HEALTH, 1);
                    SpellCastTargets targets(pVictim->GetGUID());
                    if(Spell* sor = new Spell( pVictim, sorInfo ))
                        sor->prepare(&targets, true);
                }
            }
        }

        /* -------------------------------- HONOR + BATTLEGROUND CHECKS ------------------------ */
        if( plr != NULL)
        {
            if( plr->m_bg != NULL )
                plr->m_bg->HookOnPlayerKill( plr, pVictim );
            TRIGGER_INSTANCE_EVENT( plr->GetMapMgr(), OnPlayerKillPlayer )( plr, pVictim );

            if( pVictim->IsPlayer() )
                HonorHandler::OnPlayerKilled( plr, castPtr<Player>( pVictim ) );
            else
            {
                // REPUTATION
                if( !isCritter )
                    plr->Reputation_OnKilledUnit( pVictim, false );
            }

            if(plr->getLevel() <= (pVictim->getLevel() + 8) && plr->getClass() == WARRIOR)
            {   // currently only warriors seem to use it (Victory Rush)
                plr->SetFlag( UNIT_FIELD_AURASTATE, AURASTATE_FLAG_VICTORIOUS );
                if( !sEventMgr.HasEvent(castPtr<Unit>(plr), EVENT_VICTORIOUS_FLAG_EXPIRE ) )
                    sEventMgr.AddEvent( castPtr<Unit>(plr), &Unit::EventAurastateExpire, (uint32)AURASTATE_FLAG_VICTORIOUS, EVENT_VICTORIOUS_FLAG_EXPIRE, 20000, 1, 0 );
                else
                    sEventMgr.ModifyEventTimeLeft( castPtr<Unit>(plr), EVENT_VICTORIOUS_FLAG_EXPIRE, 20000 , false );
            }
        }
        /* -------------------------------- HONOR + BATTLEGROUND CHECKS END------------------------ */

        uint64 victimGuid = pVictim->GetGUID();
        SetFlag( UNIT_FIELD_FLAGS, UNIT_FLAG_DEAD );

        // player loot for battlegrounds
        if( pVictim->IsPlayer() )
        {
            if( castPtr<Player>(pVictim)->m_bg != NULL && castPtr<Player>(pVictim)->m_bg->SupportsPlayerLoot() )
            {
                pVictim->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
                castPtr<Player>(pVictim)->m_insigniaTaken = false;
            }
        }
        else if(castPtr<Creature>(pVictim)->m_taggingPlayer != 0 )    // only execute loot code if we were tagged
        {
            // fill loot vector
            castPtr<Creature>(pVictim)->GenerateLoot();

            // update visual.
            castPtr<Creature>(pVictim)->UpdateLootAnimation(pAttacker);
        }

        if(pVictim->IsCreature())
        {
            //--------------------------------- POSSESSED CREATURES -----------------------------------------
            if( pVictim->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED_CREATURE) )
            {   //remove possess aura from controller
                Player* vController = GetMapMgr()->GetPlayer( (uint32)pVictim->GetUInt64Value(UNIT_FIELD_CHARMEDBY) );
                if( vController )
                {
                    if( vController->GetUInt64Value( UNIT_FIELD_CHARM ) == victimGuid )//make sure he is target controller
                    {
                        vController->UnPossess();
                    }
                }
            }

            //--------------------------------- PARTY LOG -----------------------------------------
            if(pVictim->IsCreature() && pVictim->GetAIInterface())
                pVictim->GetAIInterface()->OnDeath( this );

            if(IsPlayer())
            {
                WorldPacket data(SMSG_PARTYKILLLOG, 16);
                data << GetGUID() << victimGuid;
                SendMessageToSet(&data, true);
            }

            // it Seems that pets some how dont get a name and cause a crash here
            //bool isCritter = (pVictim->GetCreatureName() != NULL)? pVictim->GetCreatureName()->Type : 0;

            //---------------------------------looot-----------------------------------------
            if( IsPlayer() && !pVictim->IsPet() &&
                pVictim->GetUInt64Value( UNIT_FIELD_CREATEDBY ) == 0 &&
                pVictim->GetUInt64Value( UNIT_FIELD_SUMMONEDBY ) == 0 )
            {
                // TODO: lots of casts are bad make a temp member pointer to use for batches like this
                // that way no local loadhitstore and its just one assignment

                //Not all NPC's give XP, check for it in proto no_XP
                bool can_give_xp = true;
                if(pVictim->IsCreature())
                    if(castPtr<Creature>(pVictim)->GetExtraInfo())
                        can_give_xp = (castPtr<Creature>(pVictim)->GetExtraInfo()->no_xp ? false : true);

                if(can_give_xp)
                {
                    // Is this player part of a group
                    if( castPtr<Player>(this)->InGroup() )
                    {
                        //Calc Group XP
                        castPtr<Player>(this)->GiveGroupXP( pVictim, castPtr<Player>(this) );
                        //TODO: pet xp if player in group
                    }
                    else
                    {
                        uint32 xp = CalculateXpToGive( pVictim, castPtr<Unit>(this) );
                        if( xp > 0 )
                        {
                            if(castPtr<Player>(this)->MobXPGainRate)
                                xp += (xp*(castPtr<Player>(this)->MobXPGainRate/100));

                            castPtr<Player>(this)->GiveXP( xp, victimGuid, true );
                            if( castPtr<Player>(this)->GetSummon() && castPtr<Player>(this)->GetSummon()->GetUInt32Value( UNIT_CREATED_BY_SPELL ) == 0 )
                            {
                                xp = CalculateXpToGive( pVictim, castPtr<Player>(this)->GetSummon() );
                                if( xp > 0 )
                                    castPtr<Player>(this)->GetSummon()->GiveXP( xp );
                            }
                        }
                    }
                }

                if( pVictim->GetTypeId() != TYPEID_PLAYER )
                    sQuestMgr.OnPlayerKill( castPtr<Player>(this), castPtr<Creature>( pVictim ) );
            }
            else /* is Creature or GameObject* */
            {
                /* ----------------------------- PET XP HANDLING -------------- */
                if( owner_participe && IsPet() && !pVictim->IsPet() )
                {
                    Player* petOwner = castPtr<Pet>(this)->GetPetOwner();
                    if( petOwner != NULL && petOwner->IsPlayer() )
                    {
                        if( petOwner->InGroup() )
                        {
                            //Calc Group XP
                            castPtr<Unit>(this)->GiveGroupXP( pVictim, petOwner );
                            //TODO: pet xp if player in group
                        }
                        else
                        {
                            uint32 xp = CalculateXpToGive( pVictim, petOwner );
                            if( xp > 0 )
                            {
                                petOwner->GiveXP( xp, victimGuid, true );
                                if( !castPtr<Pet>(this)->IsSummonedPet() )
                                {
                                    xp = CalculateXpToGive( pVictim, castPtr<Pet>(this) );
                                    if( xp > 0 )
                                        castPtr<Pet>(this)->GiveXP( xp );
                                }
                            }
                        }
                    }
                    if( petOwner != NULL && pVictim->GetTypeId() != TYPEID_PLAYER &&
                        pVictim->GetTypeId() == TYPEID_UNIT )
                        sQuestMgr.OnPlayerKill( petOwner, castPtr<Creature>( pVictim ) );
                }
                /* ----------------------------- PET XP HANDLING END-------------- */

                /* ----------------------------- PET DEATH HANDLING -------------- */
                if( pVictim->IsPet() )
                {
                    // dying pet looses 1 happiness level
                    if( !castPtr<Pet>( pVictim )->IsSummonedPet() )
                    {
                        uint32 hap = castPtr<Pet>( pVictim )->GetHappiness();
                        hap = hap - PET_HAPPINESS_UPDATE_VALUE > 0 ? hap - PET_HAPPINESS_UPDATE_VALUE : 0;
                        castPtr<Pet>( pVictim )->SetHappiness(hap);
                    }

                    castPtr<Pet>( pVictim )->DelayedRemove( false, true );

                    //remove owner warlock soul link from caster
                    Player* owner = castPtr<Pet>( pVictim )->GetPetOwner();
                    if( owner != NULL )
                        owner->EventDismissPet();
                }
                /* ----------------------------- PET DEATH HANDLING END -------------- */
                else if( pVictim->GetUInt64Value( UNIT_FIELD_CHARMEDBY ) )
                {
                    //remove owner warlock soul link from caster
                    Unit* owner=pVictim->GetMapMgr()->GetUnit( pVictim->GetUInt64Value( UNIT_FIELD_CHARMEDBY ) );
                    if( owner != NULL && owner->IsPlayer())
                        castPtr<Player>( owner )->EventDismissPet();
                }
            }
        }
        else if( pVictim->IsPlayer() )
        {
            /* -------------------- REMOVE PET WHEN PLAYER DIES ---------------*/
            if( castPtr<Player>( pVictim )->GetSummon() != NULL )
            {
                if( pVictim->GetUInt32Value( UNIT_CREATED_BY_SPELL ) > 0 )
                    castPtr<Player>( pVictim )->GetSummon()->Dismiss( true );
                else
                    castPtr<Player>( pVictim )->GetSummon()->Remove( true, true, true );
            }
            /* -------------------- REMOVE PET WHEN PLAYER DIES END---------------*/
        } else sLog.outDebug("DealDamage for unknown obj.");

        return health;
    }
    else /* ---------- NOT DEAD YET --------- */
    {
        if(pVictim != castPtr<Unit>(this) /* && updateskill */)
        {
            // Send AI Reaction UNIT vs UNIT
            if( IsCreature() )
                castPtr<Creature>(this)->GetAIInterface()->AttackReaction( pVictim, damage, spellId );

            // Send AI Victim Reaction
            if( IsUnit() && pVictim->IsCreature() )
                castPtr<Creature>( pVictim )->GetAIInterface()->AttackReaction( castPtr<Unit>(this), damage, spellId );
        }

        pVictim->SetUInt32Value(UNIT_FIELD_HEALTH, health - damage );
    }
    return damage;
}

void WorldObject::SpellNonMeleeDamageLog(Unit* pVictim, uint32 spellID, uint32 damage, bool allowProc, bool no_remove_auras)
{
//==========================================================================================
//==============================Unacceptable Cases Processing===============================
//==========================================================================================
    if(!pVictim || !pVictim->isAlive())
        return;

    SpellEntry *spellInfo = dbcSpell.LookupEntry( spellID );
    if(!spellInfo)
        return;

    if (IsPlayer() && !castPtr<Player>(this)->canCast(spellInfo))
        return;
//==========================================================================================
//==============================Variables Initialization====================================
//==========================================================================================
    uint32 school = spellInfo->School;
    float res = float(damage);
    bool critical = false;
    Unit* caster = IsUnit() ? castPtr<Unit>(this) : NULL;

//==========================================================================================
//==============================+Spell Damage Bonus Calculations============================
//==========================================================================================
//------------------------------by stats----------------------------------------------------
    if( IsUnit() )
    {
        caster->m_AuraInterface.RemoveAllAurasByInterruptFlag( AURA_INTERRUPT_ON_START_ATTACK );

        res = caster->GetSpellBonusDamage( pVictim, spellInfo, 0, ( int )res, false );

//==========================================================================================
//==============================Post +SpellDamage Bonus Modifications=======================
//==========================================================================================
        if( res < 0 )
            res = 0;
        else if( !spellInfo->isUncrittableSpell() )
        {
//------------------------------critical strike chance--------------------------------------
            // lol ranged spells were using spell crit chance
            float CritChance;
            if( spellInfo->IsSpellWeaponSpell() )
            {
                CritChance = GetFloatValue( PLAYER_RANGED_CRIT_PERCENTAGE );
                CritChance -= pVictim->IsPlayer() ? castPtr<Player>(pVictim)->CalcRating( PLAYER_RATING_MODIFIER_MELEE_RESILIENCE ) : 0.0f;
            }
            else
            {
                if( spellInfo->SpellGroupType )
                {
                    caster->SM_FFValue(SMT_CRITICAL, &CritChance, spellInfo->SpellGroupType);
                    caster->SM_PFValue(SMT_CRITICAL, &CritChance, spellInfo->SpellGroupType);
                }

                CritChance -= pVictim->IsPlayer() ? castPtr<Player>(pVictim)->CalcRating( PLAYER_RATING_MODIFIER_SPELL_RESILIENCE ) : 0.0f;
            }

            if( CritChance > 0.f )
            {
                if( CritChance > 95.f )
                    CritChance = 95.f;
                critical = Rand(CritChance);
            }

//==========================================================================================
//==============================Spell Critical Hit==========================================
//==========================================================================================
            if (critical)
            {
                int32 critical_bonus = 100;
                if( spellInfo->SpellGroupType )
                    caster->SM_PIValue(SMT_CRITICAL_DAMAGE, &critical_bonus, spellInfo->SpellGroupType );

                if( critical_bonus > 0 )
                {
                    // the bonuses are halved by 50% (funky blizzard math :S)
                    float b;
                    if( spellInfo->School == 0 || spellInfo->IsSpellWeaponSpell() )     // physical || hackfix SoCommand/JoCommand
                        b = ( ( float(critical_bonus) ) / 100.0f ) + 1.0f;
                    else b = ( ( float(critical_bonus) / 2.0f ) / 100.0f ) + 1.0f;
                    res *= b;

                    if( pVictim->IsPlayer() )
                    {
                        float dmg_reduction_pct = 2.2f * castPtr<Player>(pVictim)->CalcRating( PLAYER_RATING_MODIFIER_MELEE_RESILIENCE ) / 100.0f;
                        if( dmg_reduction_pct > 0.33f )
                            dmg_reduction_pct = 0.33f; // 3.0.3

                        res = res - res * dmg_reduction_pct;
                    }
                }

                pVictim->Emote( EMOTE_ONESHOT_WOUND_CRITICAL );
            }
        }
    }
//==========================================================================================
//==============================Post Roll Calculations======================================
//==========================================================================================

//------------------------------absorption--------------------------------------------------
    uint32 abs_dmg = pVictim->AbsorbDamage(this, school, float2int32(floor(res)), dbcSpell.LookupEntry(spellID));
    res -= abs_dmg; if(res < 1.0f) res = 0.f;

    dealdamage dmg;
    dmg.school_type = school;
    dmg.full_damage = res;
    dmg.resisted_damage = abs_dmg;

    //------------------------------resistance reducing-----------------------------------------
    if(res > 0 && IsUnit())
    {
        castPtr<Unit>(this)->CalculateResistanceReduction(pVictim,&dmg,spellInfo,0.0f);
        if((int32)dmg.resisted_damage >= dmg.full_damage)
            res = 0;
        else res = float(dmg.full_damage - dmg.resisted_damage);
    }
    //------------------------------special states----------------------------------------------
    if(pVictim->bInvincible == true)
    {
        res = 0;
        dmg.resisted_damage = dmg.full_damage;
    }

//==========================================================================================
//==============================Data Sending ProcHandling===================================
//==========================================================================================

    int32 ires = float2int32(res);

//--------------------------split damage-----------------------------------------------
    SendSpellNonMeleeDamageLog(this, pVictim, spellID, ires, school, abs_dmg, dmg.resisted_damage, false, 0, critical, IsPlayer());

    if( ires > 0 ) // only deal damage if its >0
        DealDamage( pVictim, float2int32( res ), 2, 0, spellID );
    else
    {
        // we still have to tell the combat status handler we did damage so we're put in combat
        if( IsUnit() )
            castPtr<Unit>(this)->CombatStatus.OnDamageDealt(pVictim, 1);
    }

    if( (dmg.full_damage == 0 && abs_dmg) == 0 )
    {
        //Only pushback the victim current spell if it's not fully absorbed
        if( pVictim->GetCurrentSpell() )
            pVictim->GetCurrentSpell()->AddTime( school );
    }
}

//*****************************************************************************************
//* SpellLog packets just to keep the code cleaner and better to read
//*****************************************************************************************

void WorldObject::SendSpellLog(WorldObject* Caster, WorldObject* Target, uint32 Ability, uint8 SpellLogType)
{
    if ((!Caster || !Target) && Ability)
        return;

    WorldPacket data(SMSG_SPELLLOGMISS, 26);
    data << uint32(Ability);            // spellid
    data << Caster->GetGUID();          // caster / player
    data << uint8(0);                   // unknown but I think they are const
    data << uint32(1);                  // unknown but I think they are const
    data << Target->GetGUID();          // target
    data << uint8(SpellLogType);        // spelllogtype
    Caster->SendMessageToSet(&data, true);
}

void WorldObject::SendSpellNonMeleeDamageLog( WorldObject* Caster, Unit* Target, uint32 SpellID, uint32 Damage, uint8 School, uint32 AbsorbedDamage, uint32 ResistedDamage, bool PhysicalDamage, uint32 BlockedDamage, bool CriticalHit, bool bToset )
{
    if ( !Caster || !Target )
        return;
    SpellEntry *sp = dbcSpell.LookupEntry(SpellID);
    if( !sp )
        return;

    uint32 overkill = Target->computeOverkill(Damage);
    uint32 Hit_flags = (0x00001|0x00004|0x00020);
    if(CriticalHit)
        Hit_flags |= 0x00002;

    uint32 dmg = Damage-AbsorbedDamage-ResistedDamage-BlockedDamage;
    WorldPacket data(SMSG_SPELLNONMELEEDAMAGELOG, 16+4+4+4+1+4+4+1+1+4+4+1);
    data << Target->GetGUID();
    data << Caster->GetGUID();
    data << uint32(SpellID);                // SpellID / AbilityID
    data << uint32(dmg);                    // All Damage
    data << uint32(overkill);               // Overkill
    data << uint8(SchoolMask(School));      // School
    data << uint32(AbsorbedDamage);         // Absorbed Damage
    data << uint32(ResistedDamage);         // Resisted Damage
    data << uint8(PhysicalDamage ? 1 : 0);  // Physical Damage (true/false)
    data << uint8(0);                       // unknown or it binds with Physical Damage
    data << uint32(BlockedDamage);          // Physical Damage (true/false)
    data << uint32(Hit_flags);
    data << uint8(0);
    Caster->SendMessageToSet( &data, bToset );
}

int32 WorldObject::event_GetInstanceID()
{
    // return -1 for non-inworld.. so we get our shit moved to the right thread
    if(!IsInWorld())
        return -1;
    else
        return m_instanceId;
}

void WorldObject::EventSpellHit(Spell* pSpell)
{
    if( IsInWorld() && pSpell->GetCaster() != NULL )
        pSpell->cast(false);
    else pSpell->Destruct();
}

bool WorldObject::CanActivate()
{
    if(IsUnit() && !IsPet())
        return true;
    else if(IsGameObject() && castPtr<GameObject>(this)->HasAI())
        if(GetByte(GAMEOBJECT_BYTES_1, GAMEOBJECT_BYTES_TYPE_ID) != GAMEOBJECT_TYPE_TRAP)
            return true;
    return false;
}

void WorldObject::Activate(MapMgr* mgr)
{
    switch(GetTypeId())
    {
    case TYPEID_UNIT:
        if(IsVehicle())
            mgr->activeVehicles.insert(castPtr<Vehicle>(this));
        else mgr->activeCreatures.insert(castPtr<Creature>(this));
        break;

    case TYPEID_GAMEOBJECT:
        mgr->activeGameObjects.insert(castPtr<GameObject>(this));
        break;
    }

    Active = true;
}

void WorldObject::Deactivate(MapMgr* mgr)
{
    mgr->ActiveLock.Acquire();
    switch(GetTypeId())
    {
    case TYPEID_UNIT:
        {
            if(IsVehicle())
            {
                // check iterator
                if( mgr->__vehicle_iterator != mgr->activeVehicles.end() && (*mgr->__vehicle_iterator) == castPtr<Vehicle>(this) )
                    ++mgr->__vehicle_iterator;

                mgr->activeVehicles.erase(castPtr<Vehicle>(this));
            }
            else
            {
                // check iterator
                if( mgr->__creature_iterator != mgr->activeCreatures.end() && (*mgr->__creature_iterator) == castPtr<Creature>(this) )
                    ++mgr->__creature_iterator;

                mgr->activeCreatures.erase(castPtr<Creature>(this));
            }
        }break;

    case TYPEID_GAMEOBJECT:
        {
            // check iterator
            if( mgr->__gameobject_iterator != mgr->activeGameObjects.end() && (*mgr->__gameobject_iterator) == castPtr<GameObject>(this) )
                ++mgr->__gameobject_iterator;

            mgr->activeGameObjects.erase(castPtr<GameObject>(this));
        }break;
    }
    Active = false;
    mgr->ActiveLock.Release();
}

void WorldObject::SetZoneId(uint32 newZone)
{
    m_zoneId = newZone;
    if( GetTypeId() == TYPEID_PLAYER && castPtr<Player>(this)->GetGroup() )
        castPtr<Player>(this)->GetGroup()->HandlePartialChange( PARTY_UPDATE_FLAG_ZONEID, castPtr<Player>(this) );
}

// These are our hardcoded values
uint32 GetZoneForMap(uint32 mapid, uint32 areaId)
{
    switch(mapid)
    {
        // These are hardcoded values to keep data in line
    case 44: return 796;
    case 169: return 1397;
    case 449: return 1519;
    case 450: return 1637;
    case 598: return 4131;
    default:
        {
            MapEntry *entry = dbcMap.LookupEntry(mapid);
            if(entry && areaId == 0xFFFF)
                return entry->linked_zone;
        }break;
    }
    return 0;
}

void WorldObject::UpdateAreaInfo(MapMgr *mgr)
{
    m_areaFlags = OBJECT_AREA_FLAG_NONE;
    if(mgr == NULL && !IsInWorld())
    {
        m_zoneId = m_areaId = 0;
        return;
    } else if(mgr == NULL)
        mgr = GetMapMgr();

    m_zoneId = m_areaId = mgr->GetAreaID(GetPositionX(), GetPositionY(), GetPositionZ());
    if(uint32 forcedZone = GetZoneForMap(mgr->GetMapId(), m_areaId))
        m_zoneId = m_areaId = forcedZone;
    AreaTableEntry* at = dbcAreaTable.LookupEntry(m_areaId);
    if(at != NULL && at->ZoneId) // Set our Zone on add to world!
        SetZoneId(at->ZoneId);

    if(sVMapInterface.IsIndoor(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ()))
        m_areaFlags |= OBJECT_AREA_FLAG_INDOORS;
    if(sVMapInterface.IsIncity(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ()))
        m_areaFlags |= OBJECT_AREA_FLAG_INCITY;
    if(m_zoneId || m_areaId)
    {
        if(sWorld.CheckSanctuary(GetMapId(), m_zoneId, m_areaId))
            m_areaFlags |= OBJECT_AREA_FLAG_INSANCTUARY;
        AreaTableEntry* at = dbcAreaTable.LookupEntry(m_areaId);
        if(at == NULL)
            at = dbcAreaTable.LookupEntry(m_zoneId);
        if(at)
        {
            if(at->category == AREAC_CONTESTED)
                m_areaFlags |= OBJECT_AREA_FLAG_CONTESTED;
            if(at->category == AREAC_ALLIANCE_TERRITORY)
                m_areaFlags |= OBJECT_AREA_FLAG_ALLIANCE_ZONE;
            if(at->category == AREAC_HORDE_TERRITORY)
                m_areaFlags |= OBJECT_AREA_FLAG_HORDE_ZONE;
            if(at->AreaFlags & AREA_PVP_ARENA)
                m_areaFlags |= OBJECT_AREA_FLAG_ARENA_ZONE;
        }
    }
}

void WorldObject::PlaySoundToPlayer( Player* plr, uint32 sound_entry )
{
    if(plr == NULL || plr->GetSession() == NULL)
        return;

    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << sound_entry;
    plr->GetSession()->SendPacket( &data );
}

void WorldObject::PlaySoundToSet(uint32 sound_entry)
{
    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << sound_entry;
    SendMessageToSet(&data, true);
}

void WorldObject::SendAttackerStateUpdate( Unit* Target, dealdamage *dmg, uint32 realdamage, uint32 abs, uint32 blocked_damage, uint32 hit_status, uint32 vstate )
{
    if (!Target || !dmg)
        return;

    uint32 overkill = Target->computeOverkill(realdamage);
    uint32 schooldam = SchoolMask(dmg->school_type);
    WorldPacket data(SMSG_ATTACKERSTATEUPDATE, 108);
    data << uint32(hit_status);
    data << GetGUID();
    data << Target->GetGUID();
    data << uint32(realdamage);                 // Realdamage;
    data << uint32(overkill);                   // Overkill
    data << uint8(1);                           // Damage type counter / swing type
    data << uint32(schooldam);                  // Damage school
    data << float(dmg->full_damage);            // Damage float
    data << uint32(dmg->full_damage);           // Damage amount

    if(hit_status & (HITSTATUS_ABSORBED | HITSTATUS_ABSORBED2))
    {
        data << (uint32)abs;                    // Damage absorbed
    }
    if(hit_status & (HITSTATUS_RESIST | HITSTATUS_RESIST2))
    {
        data << uint32(dmg->resisted_damage);   // Damage resisted
    }

    data << uint8(vstate);                      // new victim state
    data << uint32(0);
    data << uint32(0);

    if(hit_status & HITSTATUS_BLOCK)
        data << uint32(blocked_damage);         // Damage amount blocked

    if (hit_status & 0x00800000)
        data << uint32(0);                      // unknown

    if(hit_status & HITSTATUS_unk)
    {
        data << uint32(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << float(0);
        data << uint32(0);
    }

    SendMessageToSet(&data, IsPlayer());
}

bool WorldObject::IsInLineOfSight(WorldObject* pObj)
{
    float Onoselevel = 2.0f;
    float Tnoselevel = 2.0f;
    if(IsPlayer())
        Onoselevel = castPtr<Player>(this)->m_noseLevel;
    if(pObj->IsPlayer())
        Tnoselevel = castPtr<Player>(pObj)->m_noseLevel;

    if (GetMapMgr() && GetMapMgr()->CanUseCollision(this) && GetMapMgr()->CanUseCollision(pObj))
        return (sVMapInterface.CheckLOS( GetMapId(), GetInstanceID(), GetPhaseMask(), GetPositionX(), GetPositionY(), GetPositionZ() + Onoselevel + GetFloatValue(UNIT_FIELD_HOVERHEIGHT), pObj->GetPositionX(), pObj->GetPositionY(), pObj->GetPositionZ() + Tnoselevel + pObj->GetFloatValue(UNIT_FIELD_HOVERHEIGHT)) );
    return true;
}

bool WorldObject::IsInLineOfSight(float x, float y, float z)
{
    float Onoselevel = 2.0f;
    if(IsPlayer())
        Onoselevel = castPtr<Player>(this)->m_noseLevel;

    if (GetMapMgr() && GetMapMgr()->CanUseCollision(this))
        return (sVMapInterface.CheckLOS( GetMapId(), GetInstanceID(), GetPhaseMask(), GetPositionX(), GetPositionY(), GetPositionZ() + Onoselevel + GetFloatValue(UNIT_FIELD_HOVERHEIGHT), x, y, z) );
    return true;
}

bool WorldObject::PhasedCanInteract(WorldObject* pObj)
{
    return true;
}

// Returns the base cost of a spell
int32 WorldObject::GetSpellBaseCost(SpellEntry *sp)
{
    float cost = 0.0f;
    if( sp->ManaCostPercentage && IsUnit() )//Percentage spells cost % of !!!BASE!!! mana
    {
        if( sp->powerType == POWER_TYPE_MANA)
            cost = GetUInt32Value(UNIT_FIELD_BASE_MANA) * (sp->ManaCostPercentage / 100.0f);
        else cost = GetUInt32Value(UNIT_FIELD_BASE_HEALTH) * (sp->ManaCostPercentage / 100.0f);
    } else cost = (float)sp->ManaCost;

    return float2int32(cost); // Truncate zeh decimals!
}

void WorldObject::CastSpell( WorldObject* Target, SpellEntry* Sp, bool triggered )
{
    if( Sp == NULL )
        return;

    Spell* newSpell = new Spell(this, Sp);
    SpellCastTargets targets;
    if(Target)
    {
        if(Target->IsUnit())
            targets.m_targetMask |= TARGET_FLAG_UNIT;
        else targets.m_targetMask |= TARGET_FLAG_OBJECT;
        targets.m_unitTarget = Target->GetGUID();
    } else newSpell->GenerateTargets(&targets);
    newSpell->prepare(&targets, triggered);
}

void WorldObject::CastSpell( WorldObject* Target, uint32 SpellID, bool triggered )
{
    if(SpellEntry * ent = dbcSpell.LookupEntry(SpellID))
        CastSpell(Target, ent, triggered);
}

void WorldObject::CastSpell( uint64 targetGuid, SpellEntry* Sp, bool triggered )
{
    if( Sp == NULL )
        return;

    SpellCastTargets targets(targetGuid);
    if(Spell* newSpell = new Spell(this, Sp))
        newSpell->prepare(&targets, triggered);
}

void WorldObject::CastSpell( uint64 targetGuid, uint32 SpellID, bool triggered )
{
    if(SpellEntry * ent = dbcSpell.LookupEntry(SpellID))
        CastSpell(targetGuid, ent, triggered);
}
