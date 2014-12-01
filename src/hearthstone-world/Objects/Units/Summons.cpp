/***
 * Demonstrike Core
 */

#include "StdAfx.h"

Summon::Summon(CreatureData* data, uint64 guid) : Creature(data, guid)
{
    m_Internal = NULL;
}

Summon::~Summon()
{

}

void Summon::Init()
{
    Creature::Init();
}

void Summon::Destruct()
{
    if(m_Internal != NULL)
        m_Internal->Destruct();
    Creature::Destruct();
}

void Summon::OnPushToWorld()
{
    if(m_Internal != NULL)
        m_Internal->OnPushToWorld();
    WorldObject::OnPushToWorld();
}

void Summon::CreateAs(SummonHandler* NewHandle)
{
    m_Internal = NewHandle;
    m_Internal->Initialize(this);
}

void Summon::OnRemoveInRangeObject(WorldObject* object)
{
    if(m_Internal != NULL)
        m_Internal->OnRemoveInRangeObject(object);
    if(IsTotem())
    {
        if(s_Owner == NULL || s_Owner->GetGUID() == object->GetGUID())
            event_ModifyTimeLeft(EVENT_SUMMON_EXPIRE_0+summonslot, 1);
    }

    WorldObject::OnRemoveInRangeObject(object);
}

void Summon::Load(Unit* m_owner, LocationVector & position, uint32 spellid, int32 summonslot)
{
    ASSERT(m_owner);

    Creature::Load(0, position.x, position.y, position.z, position.o);

    GetAIInterface()->Init(castPtr<Unit>(this), AITYPE_PET, MOVEMENTTYPE_NONE, m_owner);
    SetInstanceID(m_owner->GetInstanceID());
    SetFaction(m_owner->GetFactionID());
    SetZoneId(m_owner->GetZoneId());
    setLevel(m_owner->getLevel());
    SetCreatedBySpell(spellid);
    summonslot = summonslot;

    if(m_owner->IsPvPFlagged())
        SetPvPFlag();
    else
        RemovePvPFlag();

    if(m_owner->IsFFAPvPFlagged())
        SetFFAPvPFlag();
    else
        RemoveFFAPvPFlag();

    SetCreatedByGUID(m_owner->GetGUID());

    if(m_owner->IsSummon())
        SetSummonedByGUID(m_owner->GetSummonedByGUID());
    else
        SetSummonedByGUID(m_owner->GetGUID());

    if(m_owner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);

    s_Owner = m_owner;

    if(m_Internal != NULL)
        m_Internal->Load(m_owner, position, spellid, summonslot);
}

void SummonHandler::Initialize(Summon* n_summon)
{
    m_summon = n_summon;
}

void CompanionSummon::Load(Unit* owner, LocationVector & position, uint32 spellid, int32 summonslot)
{
    m_summon->SetFaction(35);
    m_summon->setLevel(1);
    m_summon->GetAIInterface()->Init(m_summon, AITYPE_PET, MOVEMENTTYPE_NONE, owner);
    m_summon->GetAIInterface()->SetUnitToFollow(owner);
    m_summon->GetAIInterface()->SetUnitToFollowAngle(-M_PI / 2);
    m_summon->GetAIInterface()->SetFollowDistance(3.0f);
    m_summon->GetAIInterface()->disable_melee = true;
    m_summon->bInvincible = true;

    m_summon->RemovePvPFlag();
    m_summon->RemoveFFAPvPFlag();
}

void GuardianSummon::Load(Unit* owner, LocationVector & position, uint32 spellid, int32 summonslot)
{
    m_summon->SetPowerType(POWER_TYPE_MANA);
    m_summon->SetMaxPower(POWER_TYPE_MANA, m_summon->GetMaxPower(POWER_TYPE_MANA) + 28 + 10 * m_summon->getLevel());
    m_summon->SetPower(POWER_TYPE_MANA, m_summon->GetPower(POWER_TYPE_MANA) + 28 + 10 * m_summon->getLevel());
    m_summon->setLevel(owner->getLevel());
    m_summon->SetMaxHealth(m_summon->GetMaxHealth() + 28 + 30 * m_summon->getLevel());
    m_summon->SetHealth(m_summon->GetMaxHealth());

    m_summon->GetAIInterface()->Init(m_summon, AITYPE_PET, MOVEMENTTYPE_NONE, owner);
    m_summon->GetAIInterface()->SetUnitToFollow(owner);
    m_summon->GetAIInterface()->SetFollowDistance(3.0f);

    m_summon->m_noRespawn = true;
}

void PossessedSummon::Load(Unit* owner, LocationVector & position, uint32 spellid, int32 summonslot)
{
    m_summon->setLevel(owner->getLevel());
    m_summon->DisableAI();
    m_summon->GetAIInterface()->StopMovement(0);
}

void TotemSummon::Load(Unit* owner, LocationVector & position, uint32 spellid, int32 summonslot)
{
    uint32 displayID = m_summon->GetCreatureData()->DisplayInfo[0];
    if( owner->IsPlayer() && castPtr<Player>(owner)->GetTeam() == 0 )
    {
        if ( m_summon->GetCreatureData()->DisplayInfo[1] != 0 )
            displayID = m_summon->GetCreatureData()->DisplayInfo[1];
        else //this is the case when you are using a blizzlike db
        {
            if( m_summon->GetCreatureData()->DisplayInfo[0] == 4587 )
                displayID = 19075;
            else if( m_summon->GetCreatureData()->DisplayInfo[0] == 4588 )
                displayID = 19073;
            else if( m_summon->GetCreatureData()->DisplayInfo[0] == 4589 )
                displayID = 19074;
            else if( m_summon->GetCreatureData()->DisplayInfo[0] == 4590 )
                displayID = 19071;
            else if( m_summon->GetCreatureData()->DisplayInfo[0] == 4683 )
                displayID = 19074;
            else displayID = m_summon->GetCreatureData()->DisplayInfo[0];
        }
    }

    // Set up the creature.
    m_summon->SetMaxPower(POWER_TYPE_FOCUS, owner->getLevel() * 30);
    m_summon->SetPower(POWER_TYPE_FOCUS, owner->getLevel() * 30);
    m_summon->setLevel(owner->getLevel());
    m_summon->setRace(0);
    m_summon->setClass(1);
    m_summon->setGender(2);
    m_summon->SetPowerType(1);
    m_summon->SetBoundingRadius(1.0f);
    m_summon->SetCombatReach(1.0f);
    m_summon->SetDisplayId(displayID);
    m_summon->SetNativeDisplayId(m_summon->GetCreatureData()->DisplayInfo[0]);
    m_summon->SetUInt32Value(UNIT_DYNAMIC_FLAGS, 0);

    m_summon->GetAIInterface()->Init(m_summon, AITYPE_TOTEM, MOVEMENTTYPE_NONE, owner);
    m_summon->DisableAI();
}

void TotemSummon::SetupSpells()
{
    if(m_summon->GetSummonOwner() == NULL)
        return;

    SpellEntry *TotemSpell = objmgr.GetTotemSpell(m_summon->GetCreatedBySpell());
    SpellEntry *creatorspell = dbcSpell.LookupEntry(m_summon->GetCreatedBySpell());
    if(TotemSpell == NULL)
    {
        sLog.outError("Totem %u does not have any spells to cast", m_summon->GetEntry());
        return;
    }

    // Set up AI, depending on our spells.
    bool castingtotem = true;

    if((SP_HasEffect(TotemSpell, SPELL_EFFECT_SUMMON)
        || SP_HasEffect(TotemSpell, SPELL_EFFECT_APPLY_AURA)
        || SP_HasEffect(TotemSpell, SPELL_EFFECT_HEALTH_FUNNEL)
        || SP_HasEffect(TotemSpell, SPELL_EFFECT_APPLY_AREA_AURA)
        || SP_HasEffect(TotemSpell, SPELL_EFFECT_PERSISTENT_AREA_AURA))
        && SP_AppliesAura(TotemSpell, SPELL_AURA_PERIODIC_TRIGGER_SPELL))
        castingtotem = false;

    if(!castingtotem)
    {
        // We're an area aura. Simply cast the spell.
        SpellCastTargets targets;

        if(!SP_AppliesAura(TotemSpell, SPELL_AURA_PERIODIC_TRIGGER_SPELL))
        {
            targets.m_destX = m_summon->GetPositionX();
            targets.m_destY = m_summon->GetPositionY();
            targets.m_destZ = m_summon->GetPositionZ();
            targets.m_targetMask = TARGET_FLAG_DEST_LOCATION;
        }
        (new Spell(m_summon, TotemSpell, true, 0))->prepare(&targets);
    }
    else
    {   // We're a casting totem. Switch AI on, and tell it to cast this spell.
        m_summon->EnableAI();
        m_summon->GetAIInterface()->m_totemSpell = TotemSpell;
        m_summon->GetAIInterface()->m_totemSpellTime = 3 * MSTIME_SECOND;
    }
}

void WildSummon::Load(Unit* owner, LocationVector & position, uint32 spellid, int32 summonslot)
{
    m_summon->setLevel(owner->getLevel());
}

void Spell::SpellEffectSummon(uint32 i)
{
    if(u_caster == NULL )
         return;

    SummonPropertiesEntry * spe = dbcSummonProperties.LookupEntry( GetSpellProto()->EffectMiscValueB[i] );
    if( spe == NULL )
        return;

    uint32 entry = m_spellInfo->EffectMiscValue[ i ];

    CreatureData* ctrData = sCreatureDataMgr.GetCreatureData(entry);
    if(ctrData == NULL)
    {
        sLog.outError("Spell %u ( %s ) tried to summon creature %u without database data", m_spellInfo->Id, m_spellInfo->Name, entry);
        return;
    }

    if(damage == 0)
        damage = 1;

    LocationVector v(0.0f, 0.0f, 0.0f, 0.0f);

    if((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) != 0)
        v = LocationVector(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ);
    else v = u_caster->GetPosition();

    // Client adds these spells to the companion window, it's weird but then it happens anyways
    if(spe->slot == 5)
    {
        SummonCompanion(i, spe, ctrData, v);
        return;
    }

    // Delete any objects in my slots. Slot 0 can have unlimited objects.
    if( spe->slot != 0 && spe->slot < 7 && u_caster->m_Summons[ spe->slot ].size() )
        u_caster->SummonExpireSlot(spe->slot);

    switch( spe->controltype )
    {
    case DBC_SUMMON_CONTROL_TYPE_GUARDIAN:
        {
            if(spe->Id == 121)
            {
                SummonTotem(i, spe, ctrData, v);
                return;
            }
        }break;
    case DBC_SUMMON_CONTROL_TYPE_PET:
        {
            SummonTemporaryPet(i, spe, ctrData, v);
            return;
        }break;
    case DBC_SUMMON_CONTROL_TYPE_POSSESSED:
        {
            SummonPossessed(i, spe, ctrData, v);
            return;
        }break;
    case DBC_SUMMON_CONTROL_TYPE_VEHICLE:
        {
            SummonVehicle( i, spe, ctrData, v );
            return;
        }break;
    }

    switch( spe->type )
    {
    case DBC_SUMMON_TYPE_NONE:
    case DBC_SUMMON_TYPE_CONSTRUCT:
    case DBC_SUMMON_TYPE_OPPONENT:
        {
            if(spe->controltype == DBC_SUMMON_CONTROL_TYPE_GUARDIAN)
                SummonGuardian(i, spe, ctrData, v);
            else SummonWild(i, spe, ctrData, v);
            return;
        }break;
    case DBC_SUMMON_TYPE_PET:
        {
            SummonTemporaryPet(i, spe, ctrData, v);
        }break;
    case DBC_SUMMON_TYPE_GUARDIAN:
    case DBC_SUMMON_TYPE_MINION:
    case DBC_SUMMON_TYPE_RUNEBLADE:
        {
            SummonGuardian(i, spe, ctrData, v);
        }break;
    case DBC_SUMMON_TYPE_TOTEM:
        {
            SummonTotem(i, spe, ctrData, v);
        }break;
    case DBC_SUMMON_TYPE_COMPANION:
        {   // These are used as guardians in some quests
            if(spe->slot == 6)
                SummonGuardian(i, spe, ctrData, v);
            else SummonCompanion(i, spe, ctrData, v);
        }break;
    case DBC_SUMMON_TYPE_VEHICLE:
    case DBC_SUMMON_TYPE_MOUNT:
        {
            SummonVehicle( i, spe, ctrData, v );
        }break;
    case DBC_SUMMON_TYPE_LIGHTWELL:
        {
            SummonGuardian(i, spe, ctrData, v);
        }break;
    default:
        {
            sLog.outError("Spell Id %u, has an invalid summon type %u report this to devs.", GetSpellProto()->Id, spe->Id);
        }break;
    }
}

void Spell::SummonWild(uint32 i, SummonPropertiesEntry * Properties, CreatureData *data, LocationVector & v)
{
    if(g_caster != NULL)
        u_caster = g_caster->m_summoner;

    if(u_caster == NULL)
        return;

    int32 duration = GetDuration();
    uint32 slot = Properties ? Properties->slot : 0;
    for(int32 j = 0; j < damage; j++)
    {
        LocationVector SpawnLocation(v);
        float followangle = -1 * M_PI / 2 * j;
        SpawnLocation.x += (GetRadius(i) * (cosf(followangle + v.o)));
        SpawnLocation.y += (GetRadius(i) * (sinf(followangle + v.o)));
        followangle = (u_caster->calcAngle(u_caster->GetPositionX(), u_caster->GetPositionY(), SpawnLocation.x, SpawnLocation.y) * float(M_PI) / 180.0f);

        Summon* s = u_caster->GetMapMgr()->CreateSummon(data->Entry);
        if(s == NULL)
            return;

        u_caster->AddSummonToSlot(slot, s);
        s->CreateAs(new WildSummon());
        s->Load(u_caster, SpawnLocation, m_spellInfo->Id, slot);
        s->GetAIInterface()->SetUnitToFollowAngle(followangle);
        s->PushToWorld(u_caster->GetMapMgr());

        if(Properties)
        {
            if((p_caster != NULL) && (Properties->slot != 0))
            {
                WorldPacket data(SMSG_TOTEM_CREATED, 17);
                data << uint8(Properties->slot - 1);
                data << uint64(s->GetGUID());
                data << int32(duration);
                data << uint32(m_spellInfo->Id);
                p_caster->SendPacket(&data);
            }
        }
    }

    if(duration > 0)
        sEventMgr.AddEvent(u_caster, &Unit::SummonExpireSlot, uint8(slot), EVENT_SUMMON_EXPIRE_0+slot, duration, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void Spell::SummonTotem(uint32 i, SummonPropertiesEntry * Properties, CreatureData *data, LocationVector & v)
{
    if(u_caster == NULL)
        return;

    uint32 slot = Properties ? Properties->slot : 1;
    bool xSubtractX = true;
    bool xSubtractY = true;
    if(slot < 3)
        xSubtractX = false;
    if((slot/2)*2 == slot)
        xSubtractY = false;

    v.x += xSubtractX ? -1.5f : 1.5f;
    v.y += xSubtractY ? -1.5f : 1.5f;

    float landh = u_caster->GetCHeightForPosition(false, v.x, v.y, v.z + 2);
    float landdiff = landh - v.z;

    if(fabs(landdiff) <= 15)
        v.z = landh;

    Summon* s = u_caster->GetMapMgr()->CreateSummon(data->Entry);
    if(s == NULL)
        return;

    u_caster->AddSummonToSlot(slot, s);
    s->CreateAs(new TotemSummon());
    s->Load(u_caster, v, m_spellInfo->Id, slot);
    s->SetMaxHealth(damage);
    s->SetHealth(damage);
    s->PushToWorld(u_caster->GetMapMgr());

    int32 duration = GetDuration();
    if(p_caster != NULL)
    {
        WorldPacket data(SMSG_TOTEM_CREATED, 17);
        data << uint8(slot - 1);
        data << uint64(s->GetGUID());
        data << int32(duration);
        data << uint32(m_spellInfo->Id);
        p_caster->SendPacket(&data);
    }

    if(duration > 0)
        sEventMgr.AddEvent(u_caster, &Unit::SummonExpireSlot, uint8(slot), EVENT_SUMMON_EXPIRE_0+slot, duration, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
    else sEventMgr.AddEvent(u_caster, &Unit::SummonExpireSlot, uint8(slot), EVENT_SUMMON_EXPIRE_0+slot, 60 * 60 * 1000, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void Spell::SummonGuardian(uint32 i, SummonPropertiesEntry * Properties, CreatureData *data, LocationVector & v) // Summon Guardian
{
    if(g_caster != NULL)
        u_caster = g_caster->m_summoner;

    if(u_caster == NULL)
        return;

    float angle_for_each_spawn = -float(float(M_PI * 2) / damage);

    // it's health., or a fucked up infernal.
    if( Properties == NULL )
        damage = 1;
    else if( damage < 1 || (Properties->unk2 & 2 || Properties->Id == 711))
        damage = 1;

    if(data->Entry == 31216) // mirror image
        damage = 3;

    int32 duration = GetDuration();
    uint32 slot = Properties ? Properties->slot : 0;
    for(int j = 0; j < damage; j++)
    {
        LocationVector SpawnLocation(v);
        float followangle = angle_for_each_spawn * j;
        SpawnLocation.x += (GetRadius(i) * (cosf(followangle)));
        SpawnLocation.y += (GetRadius(i) * (sinf(followangle)));
        followangle = (u_caster->calcAngle(u_caster->GetPositionX(), u_caster->GetPositionY(), SpawnLocation.x, SpawnLocation.y) * float(M_PI / ((180/damage) * (j+1))));

        Summon* s = u_caster->GetMapMgr()->CreateSummon(data->Entry);
        if(s == NULL)
            return;

        u_caster->AddSummonToSlot(slot, s);
        s->CreateAs(new GuardianSummon());
        s->Load(u_caster, SpawnLocation, m_spellInfo->Id, slot);
        s->GetAIInterface()->SetUnitToFollowAngle(followangle);
        s->PushToWorld(u_caster->GetMapMgr());

        if((p_caster != NULL) && (slot != 0))
        {
            WorldPacket data(SMSG_TOTEM_CREATED, 17);
            data << uint8(slot - 1);
            data << uint64(s->GetGUID());
            data << int32(duration);
            data << uint32(m_spellInfo->Id);
            p_caster->SendPacket(&data);
        }

        // Lightwell
        if(Properties != NULL)
        {
            if(Properties->type == DBC_SUMMON_TYPE_LIGHTWELL)
            {
                s->Root();
                s->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK);
            }
        }
    }

    if(duration > 0)
        sEventMgr.AddEvent(u_caster, &Unit::SummonExpireSlot, uint8(slot), EVENT_SUMMON_EXPIRE_0+slot, duration, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void Spell::SummonTemporaryPet(uint32 i, SummonPropertiesEntry * Properties, CreatureData *data, LocationVector & v)
{
    if(p_caster == NULL)
        return;

    if(p_caster->GetSummon())
    {
        if(p_caster->GetSummon()->GetUInt32Value(UNIT_CREATED_BY_SPELL) > 0)
            p_caster->GetSummon()->Dismiss(false);              // warlock summon -> dismiss
        else
            p_caster->GetSummon()->Remove(false, true, true);   // hunter pet -> just remove for later re-call
    }

    int32 count = damage;
    if(m_spellInfo->Id == 51533)
        count = 2;
    else if(Properties != NULL && Properties->Id == 711) // Only Inferno uses this SummonProperty ID, and somehow it has the wrong count
        count = 1;

    float angle_for_each_spawn = -M_PI * 2 / damage;

    int32 duration = GetDuration();
    LocationVector spawnLoc;
    uint32 slot = Properties ? Properties->slot : 0;
    for(int32 i = 0; i < count; i++)
    {
        Pet* pet = objmgr.CreatePet(data);
        if(pet == NULL)
            continue;

        float followangle = angle_for_each_spawn * i;
        spawnLoc.ChangeCoords(v.x, v.y, v.z);
        spawnLoc.x += 3 * (cosf(followangle + u_caster->GetOrientation()));
        spawnLoc.y += 3 * (sinf(followangle + u_caster->GetOrientation()));
        followangle = (u_caster->calcAngle(u_caster->GetPositionX(), u_caster->GetPositionY(), spawnLoc.x, spawnLoc.y) * float(M_PI) / 180.0f);

        u_caster->AddSummonToSlot(slot, pet);
        pet->CreateAsSummon(NULL, p_caster, &spawnLoc, m_spellInfo, 1, duration);
        pet->GetAIInterface()->SetUnitToFollowAngle(followangle);
    }

    if(duration > 0)
        sEventMgr.AddEvent(u_caster, &Unit::SummonExpireSlot, uint8(slot), EVENT_SUMMON_EXPIRE_0+slot, duration, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void Spell::SummonPossessed(uint32 i, SummonPropertiesEntry * Properties, CreatureData *data, LocationVector & v)
{
    if(p_caster == NULL)
        return;

    if(p_caster->GetSummon())
    {
        if(p_caster->GetSummon()->GetUInt32Value(UNIT_CREATED_BY_SPELL) > 0)
            p_caster->GetSummon()->Dismiss(false);              // warlock summon -> dismiss
        else p_caster->GetSummon()->Remove(false, true, true);   // hunter pet -> just remove for later re-call
    }

    Summon* s = p_caster->GetMapMgr()->CreateSummon(data->Entry);
    if(s == NULL)
        return;

    v.x += (3 * cos(M_PI / 2 + v.o));
    v.y += (3 * cos(M_PI / 2 + v.o));

    int32 duration = GetDuration();
    uint32 slot = Properties ? Properties->slot : 0;
    s->CreateAs(new PossessedSummon());
    s->Load(p_caster, v, m_spellInfo->Id, slot);
    s->SetCreatedBySpell(m_spellInfo->Id);
    s->PushToWorld(p_caster->GetMapMgr());

    p_caster->Possess(s);

    if(duration > 0)
        sEventMgr.AddEvent(u_caster, &Unit::SummonExpireSlot, uint8(slot), EVENT_SUMMON_EXPIRE_0+slot, duration, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void Spell::SummonCompanion(uint32 i, SummonPropertiesEntry * Properties, CreatureData *data, LocationVector & v)
{
    if(u_caster == NULL)
        return;

    if(u_caster->GetSummonedCritterGUID() != 0)
    {
        Creature* critter = u_caster->GetMapMgr()->GetCreature(u_caster->GetSummonedCritterGUID());
        uint32 currententry = critter->GetEntry();
        critter->RemoveFromWorld(false, true);
        u_caster->SetSummonedCritterGUID(0);

        // Before WOTLK when you casted the companion summon spell the second time it removed the companion
        // Customized servers or old databases could still use this method
        if(data->Entry == currententry)
            return;
    }

    Summon* s = u_caster->GetMapMgr()->CreateSummon(data->Entry);
    if(s == NULL)
        return;

    int32 duration = GetDuration();
    uint32 slot = Properties ? Properties->slot : 0;
    s->CreateAs(new CompanionSummon());
    s->Load(u_caster, v, m_spellInfo->Id, slot);
    s->SetCreatedBySpell(m_spellInfo->Id);
    s->GetAIInterface()->SetFollowDistance(GetRadius(i));
    s->PushToWorld(u_caster->GetMapMgr());
    u_caster->SetSummonedCritterGUID(s->GetGUID());

    if(duration > 0)
        sEventMgr.AddEvent(u_caster, &Unit::SummonExpireSlot, uint8(slot), EVENT_SUMMON_EXPIRE_0+slot, duration, 1, EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void Spell::SummonVehicle(uint32 i, SummonPropertiesEntry * Properties, CreatureData *data, LocationVector & v)
{
    if( u_caster == NULL )
        return;

    // If it has no vehicle id, then we can't really do anything with it as a vehicle :/
    if( data->Vehicle_entry == 0 )
        return;

    Vehicle *veh = u_caster->GetMapMgr()->CreateVehicle( data->Entry );
    veh->Load(u_caster->GetMapMgr()->iInstanceMode, v.x, v.y, v.z, v.o);
    veh->SetCreatedBySpell( m_spellInfo->Id );
    veh->SetCreatedByGUID( u_caster->GetGUID() );
    veh->SetSummonedByGUID( u_caster->GetGUID() );
    veh->RemoveFlag( UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK );
    veh->PushToWorld( u_caster->GetMapMgr() );

    // Need to delay this a bit since first the client needs to see the vehicle
    veh->AddPassenger(u_caster, 0, true);
}