/***
 * Demonstrike Core
 */

#include "StdAfx.h"

#define SPELL_CHANNEL_UPDATE_INTERVAL 1000

/// externals for spell system
extern pSpellEffect SpellEffectsHandler[TOTAL_SPELL_EFFECTS];

bool SP_HasEffect(SpellEntry* spellInfo, uint32 EffectId)
{
    if(spellInfo == NULL)
        return false;

    if(spellInfo->Effect[0] == EffectId)
        return true;
    if(spellInfo->Effect[1] == EffectId)
        return true;
    if(spellInfo->Effect[2] == EffectId)
        return true;
    return false;
}

bool SP_AppliesAura(SpellEntry* spellInfo, uint32 AuraName)
{
    if(spellInfo == NULL)
        return false;

    if(spellInfo->EffectApplyAuraName[0] == AuraName)
        return true;
    if(spellInfo->EffectApplyAuraName[1] == AuraName)
        return true;
    if(spellInfo->EffectApplyAuraName[2] == AuraName)
        return true;
    return false;
}

int32 GetSpellInfoDuration(SpellEntry* m_spellInfo, Unit* u_caster, Unit* unitTarget)
{
    int32 m_duration = -1, c_dur = 0;
    if(m_spellInfo->DurationIndex)
    {
        SpellDuration *sd = dbcSpellDuration.LookupEntry(m_spellInfo->DurationIndex);
        if(sd == NULL)
            return m_duration;

        m_duration = sd->Duration1;
        //check for negative and 0 durations.
        //duration affected by level
        if(sd->Duration1 <= 0 && sd->Duration2 > 0 && u_caster)
        {
            m_duration = uint32((sd->Duration1 + (sd->Duration2 * u_caster->getLevel())));
            if(m_duration > 0 && sd->Duration3 > 0 && m_duration > sd->Duration3)
            {
                m_duration = sd->Duration3;
            }

            if(m_duration < 0)
                m_duration = 0;
            c_dur = m_duration;
        }

        if(u_caster)
        {
            //combo point lolerCopter? ;P
            if(u_caster->IsPlayer() && sd->Duration3)
            {
                uint32 cp = castPtr<Player>(u_caster)->m_comboPoints;
                if(cp)
                {
                    uint32 bonus = (cp*(sd->Duration3-sd->Duration1))/5;
                    if(bonus)
                        m_duration += bonus;
                }
            }

            if(m_spellInfo->SpellGroupType)
            {
                u_caster->SM_FIValue(SMT_DURATION, (int32*)&m_duration, m_spellInfo->SpellGroupType);
                u_caster->SM_PIValue(SMT_DURATION, (int32*)&m_duration, m_spellInfo->SpellGroupType);
            }
        }
    }

    return m_duration;
}


enum SpellTargetSpecification
{
    TARGET_SPECT_NONE       = 0,
    TARGET_SPEC_INVISIBLE   = 1,
    TARGET_SPEC_DEAD        = 2,
};

void SpellCastTargets::read( WorldPacket & data, uint64 caster )
{
    data >> m_targetIndex >> m_castFlags >> m_targetMask;
    if( m_targetMask == TARGET_FLAG_SELF || m_targetMask & TARGET_FLAG_GLYPH )
        m_unitTarget = caster;

    if( m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_UNIT | TARGET_FLAG_CORPSE | TARGET_FLAG_CORPSE2 ) )
        data >> m_unitTarget.asPacked();
    else if( m_targetMask & ( TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM ) )
        data >> m_unitTarget.asPacked();

    if( m_targetMask & TARGET_FLAG_SOURCE_LOCATION )
    {
        data >> m_src_transGuid.asPacked() >> m_srcX >> m_srcY >> m_srcZ;
        if( !( m_targetMask & TARGET_FLAG_DEST_LOCATION ) )
        {
            m_dest_transGuid = m_src_transGuid;
            m_destX = m_srcX;
            m_destY = m_srcY;
            m_destZ = m_srcZ;
        }
    }

    if( m_targetMask & TARGET_FLAG_DEST_LOCATION )
    {
        data >> m_dest_transGuid.asPacked() >> m_destX >> m_destY >> m_destZ;
        if( !( m_targetMask & TARGET_FLAG_SOURCE_LOCATION ) )
        {
            m_src_transGuid = m_dest_transGuid;
            m_srcX = m_destX;
            m_srcY = m_destY;
            m_srcZ = m_destZ;
        }
    }

    if( m_targetMask & TARGET_FLAG_STRING )
        data >> m_strTarget;

    if(m_castFlags & 0x2)
    {
        data >> missilepitch >> missilespeed;
        if(data.read<uint8>() == 1)
            data.read_skip<uint64>();

        float dx = m_destX - m_srcX;
        float dy = m_destY - m_srcY;
        if((missilepitch != (M_PI / 4)) && (missilepitch != -M_PI / 4))
            traveltime = (sqrtf(dx * dx + dy * dy) / (cosf(missilepitch) * missilespeed)) * 1000;
    }
}

void SpellCastTargets::write( WorldPacket& data )
{
    data << m_targetMask;

    if( m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_CORPSE | TARGET_FLAG_CORPSE2 | TARGET_FLAG_OBJECT | TARGET_FLAG_GLYPH) )
        FastGUIDPack( data, m_unitTarget );

    if( m_targetMask & ( TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM ) )
        FastGUIDPack( data, m_itemTarget );

    if( m_targetMask & TARGET_FLAG_SOURCE_LOCATION )
        data << m_src_transGuid << m_srcX << m_srcY << m_srcZ;

    if( m_targetMask & TARGET_FLAG_DEST_LOCATION )
        data << m_dest_transGuid << m_destX << m_destY << m_destZ;

    if (m_targetMask & TARGET_FLAG_STRING)
        data << m_strTarget;
}

Spell::Spell(WorldObject* Caster, SpellEntry *info, bool triggered, Aura* aur)
{
    ASSERT( Caster != NULL && info != NULL );

    chaindamage = 0;
    m_spellInfo = info;
    m_caster = Caster;
    duelSpell = false;
    m_pushbackCount = 0;
    m_missilePitch = 0;
    m_missileTravelTime = 0;
    MSTimeToAddToTravel = 0;

    switch( Caster->GetTypeId() )
    {
    case TYPEID_PLAYER:
        {
            g_caster = NULL;
            i_caster = NULL;
            u_caster = castPtr<Unit>( Caster );
            p_caster = castPtr<Player>( Caster );
            v_caster = NULL;
            if(Caster->IsVehicle())
                v_caster = castPtr<Vehicle>( Caster );
            if( p_caster->GetDuelState() == DUEL_STATE_STARTED )
                duelSpell = true;
        }break;

    case TYPEID_UNIT:
        {
            g_caster = NULL;
            i_caster = NULL;
            p_caster = NULL;
            v_caster = NULL;
            u_caster = castPtr<Unit>( Caster );
            if(Caster->IsVehicle())
            {
                v_caster = castPtr<Vehicle>( Caster );
                if(!p_caster)
                    p_caster = v_caster->m_redirectSpellPackets;
            }
            if( u_caster->IsPet() && castPtr<Pet>( u_caster)->GetPetOwner() != NULL && castPtr<Pet>( u_caster )->GetPetOwner()->GetDuelState() == DUEL_STATE_STARTED )
                duelSpell = true;
        }break;

    case TYPEID_ITEM:
    case TYPEID_CONTAINER:
        {
            g_caster = NULL;
            u_caster = NULL;
            p_caster = NULL;
            v_caster = NULL;
            i_caster = castPtr<Item>( Caster );
            if( i_caster->GetOwner() && i_caster->GetOwner()->GetDuelState() == DUEL_STATE_STARTED )
                duelSpell = true;
        }break;

    case TYPEID_GAMEOBJECT:
        {
            u_caster = NULL;
            p_caster = NULL;
            v_caster = NULL;
            i_caster = NULL;
            g_caster = castPtr<GameObject>( Caster );
        }break;

    default:
        {
            sLog.outDebug("[DEBUG][SPELL] Incompatible object type, please report this to the dev's");
        }break;
    }

    m_spellState = SPELL_STATE_NULL;

    m_castPositionX = m_castPositionY = m_castPositionZ = 0;
    m_triggeredSpell = triggered;
    m_AreaAura = false;
    static_damage = false;

    m_triggeredByAura = aur;

    AdditionalCritChance = 0;
    m_ForcedCastTime = 0;
    damageToHit = 0;
    castedItemId = 0;

    m_usesMana = false;
    m_Spell_Failed = false;
    bDurSet = false;
    bRadSet[0] = false;
    bRadSet[1] = false;
    bRadSet[2] = false;

    cancastresult = SPELL_CANCAST_OK;

    unitTarget = NULL;
    itemTarget = NULL;
    gameObjTarget = NULL;
    playerTarget = NULL;
    corpseTarget = NULL;
    damage = 0;
    add_damage = 0;
    TotalDamage = 0;
    m_Delayed = false;
    m_ForceConsumption = false;
    m_triggeredSpellId = 0;
    m_cancelled = false;
    ProcedOnSpell = NULL;
    extra_cast_number = 0;
    m_reflectedParent = NULL;
    m_isCasting = false;
    m_hitTargetCount = 0;
    m_missTargetCount = 0;
    m_magnetTarget = 0;
    m_projectileWait = false;
}

Spell::~Spell()
{

}

void Spell::Destruct()
{
    if( u_caster != NULL && u_caster->GetCurrentSpell() == this )
        u_caster->SetCurrentSpell(NULL);
    for(std::map<uint64, Aura*>::iterator itr = m_tempAuras.begin(); itr != m_tempAuras.end(); itr++)
        delete itr->second;
    m_tempAuras.clear();

    v_caster = NULL;
    g_caster = NULL;
    u_caster = NULL;
    i_caster = NULL;
    p_caster = NULL;
    m_caster = NULL;
    v_caster = NULL;
    m_triggeredByAura = NULL;
    unitTarget = NULL;
    itemTarget = NULL;
    gameObjTarget = NULL;
    playerTarget = NULL;
    corpseTarget = NULL;
    m_magnetTarget = NULL;
    m_reflectedParent = NULL;
    EventableObject::Destruct();
}

bool Spell::IsAuraApplyingSpell()
{
    if(m_spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA))
        return true;
    for(uint8 i = 0; i < 3; i++)
        if(GetSpellProto()->EffectApplyAuraName[i])
            return true;
    return false;
}

//i might forget conditions here. Feel free to add them
bool Spell::IsStealthSpell()
{
    //check if aura name is some stealth aura
    if( GetSpellProto()->EffectApplyAuraName[0] == 16 ||
        GetSpellProto()->EffectApplyAuraName[1] == 16 ||
        GetSpellProto()->EffectApplyAuraName[2] == 16 )
        return true;
    return false;
}

//i might forget conditions here. Feel free to add them
bool Spell::IsInvisibilitySpell()
{
    //check if aura name is some invisibility aura
    if( GetSpellProto()->EffectApplyAuraName[0] == 18 ||
        GetSpellProto()->EffectApplyAuraName[1] == 18 ||
        GetSpellProto()->EffectApplyAuraName[2] == 18 )
        return true;
    return false;
}

void Spell::FillSpecifiedTargetsInArea( float srcx, float srcy, float srcz, uint32 ind, uint32 specification )
{
    FillSpecifiedTargetsInArea( ind, srcx, srcy, srcz, GetRadius(ind), specification );
}

// for the moment we do invisible targets
void Spell::FillSpecifiedTargetsInArea(uint32 i,float srcx,float srcy,float srcz, float range, uint32 specification)
{
    //TargetsList *tmpMap=&m_targetUnits[i];
    //InStealth()
    float r = range * range;
    //uint8 did_hit_result;
    for(std::unordered_set<WorldObject* >::iterator itr = m_caster->GetInRangeSetBegin(); itr != m_caster->GetInRangeSetEnd(); itr++ )
    {
        // don't add objects that are units and dead
        if( (*itr)->IsUnit() && !(castPtr<Unit>( *itr )->isAlive()))
            continue;

        //castPtr<Unit>(*itr)->InStealth()
        if( GetSpellProto()->TargetCreatureType && (*itr)->IsUnit())
        {
            Unit* Target = castPtr<Unit>((*itr));
            if(!(1<<(Target->GetCreatureType()-1) & GetSpellProto()->TargetCreatureType))
                continue;
        }

        if(IsInrange(srcx,srcy,srcz,(*itr),r))
        {
            if( v_caster != NULL ) // Vehicles can destroy gameobjects
            {
                if( (*itr)->IsUnit() )
                {
                    if( sFactionSystem.isAttackable( u_caster, castPtr<Unit>( *itr ),!GetSpellProto()->isSpellStealthTargetCapable() ) )
                    {
                        _AddTarget((castPtr<Unit>(*itr)), i);
                    }
                }
                else if((*itr)->IsGameObject())
                {
                    _AddTargetForced((*itr), i);
                }
            }
            else if( u_caster != NULL )
            {
                if( (*itr)->IsUnit() )
                {
                    if( sFactionSystem.isAttackable( u_caster, castPtr<Unit>( *itr ),!GetSpellProto()->isSpellStealthTargetCapable() ) )
                    {
                        _AddTarget((castPtr<Unit>(*itr)), i);
                    }
                }
            }
            else //cast from GO
            {
                if(g_caster && g_caster->GetUInt64Value(GAMEOBJECT_FIELD_CREATED_BY) && g_caster->m_summoner)
                {
                    if((*itr)->IsUnit())
                    {   //trap, check not to attack owner and friendly
                        if(sFactionSystem.isAttackable(g_caster->m_summoner,castPtr<Unit>(*itr),!GetSpellProto()->isSpellStealthTargetCapable() ) )
                            _AddTarget((castPtr<Unit>(*itr)), i);
                    }
                }
                else
                    _AddTargetForced((*itr), i);
            }
            if( GetSpellProto()->MaxTargets)
            {
                if( m_hitTargetCount >= GetSpellProto()->MaxTargets )
                    return;
            }
        }
    }
}
void Spell::FillAllTargetsInArea(LocationVector & location,uint32 ind)
{
    FillAllTargetsInArea(ind,location.x,location.y,location.z,GetRadius(ind));
}

void Spell::FillAllTargetsInArea(float srcx,float srcy,float srcz,uint32 ind)
{
    FillAllTargetsInArea(ind,srcx,srcy,srcz,GetRadius(ind));
}

/// We fill all the targets in the area, including the stealth ed one's
void Spell::FillAllTargetsInArea(uint32 i,float srcx,float srcy,float srcz, float range, bool includegameobjects)
{
    //TargetsList *tmpMap=&m_targetUnits[i];
    float r = range*range;
    uint32 placeholder = 0;
    std::vector<WorldObject*> ChainTargetContainer;

    for(std::unordered_set<WorldObject* >::iterator itr = m_caster->GetInRangeSetBegin(); itr != m_caster->GetInRangeSetEnd(); itr++ )
    {
        // don't add objects that are units and dead
        if( (*itr)->IsUnit() && (!(castPtr<Unit>( *itr )->isAlive())))
            continue;

        if( GetSpellProto()->TargetCreatureType && (*itr)->IsUnit())
        {
            Unit* Target = castPtr<Unit>((*itr));
            if(!(1<<(Target->GetCreatureType()-1) & GetSpellProto()->TargetCreatureType))
                continue;
        }

        if(IsInrange(srcx,srcy,srcz,(*itr),r))
        {
            if( includegameobjects )
            {
                if( (*itr)->IsUnit() )
                {
                    if( sFactionSystem.isAttackable( m_caster, castPtr<Unit>( *itr ),!GetSpellProto()->isSpellStealthTargetCapable() ) )
                    {
                        ChainTargetContainer.push_back(*itr);
                        placeholder++;
                    }
                }
                else if((*itr)->IsGameObject())
                {
                    ChainTargetContainer.push_back(*itr);
                    placeholder++;
                }
            }
            else if( u_caster != NULL )
            {
                if( (*itr)->IsUnit() )
                {
                    if( sFactionSystem.isAttackable( u_caster, castPtr<Unit>( *itr ),!GetSpellProto()->isSpellStealthTargetCapable() ) )
                    {
                        ChainTargetContainer.push_back(*itr);
                        placeholder++;
                    }
                }
            }
            else //cast from GO
            {
                if(g_caster && g_caster->GetUInt64Value(GAMEOBJECT_FIELD_CREATED_BY) && g_caster->m_summoner)
                {
                    if((*itr)->IsUnit())
                    {   //trap, check not to attack owner and friendly
                        if(sFactionSystem.isAttackable(g_caster->m_summoner,castPtr<Unit>(*itr),!GetSpellProto()->isSpellStealthTargetCapable() ) )
                        {
                            ChainTargetContainer.push_back(*itr);
                            placeholder++;
                        }
                    }
                }
                else
                {
                    ChainTargetContainer.push_back(*itr);
                    placeholder++;
                }
            }
        }
    }

    int32 MaxTargets = -1;
    if( GetSpellProto()->MaxTargets)
        MaxTargets = GetSpellProto()->MaxTargets-1;

    if(MaxTargets != -1)
    {
        WorldObject* chaintarget = NULL;
        while(MaxTargets && ChainTargetContainer.size())
        {
            placeholder = (rand()%ChainTargetContainer.size());
            chaintarget = ChainTargetContainer.at(placeholder);
            if(chaintarget == NULL)
                continue;

            _AddTargetForced(chaintarget, i);
            ChainTargetContainer.erase(ChainTargetContainer.begin()+placeholder);
            MaxTargets--;
        }
    }
    else
    {
        for(std::vector<WorldObject*>::iterator itr = ChainTargetContainer.begin(); itr != ChainTargetContainer.end(); itr++)
            _AddTargetForced((*itr), i);
    }
    ChainTargetContainer.clear();
}

// We fill all the targets in the area, including the stealthed one's
void Spell::FillAllFriendlyInArea( uint32 i, float srcx, float srcy, float srcz, float range )
{
    float r = range*range;

    for( std::unordered_set<WorldObject* >::iterator itr = m_caster->GetInRangeSetBegin(); itr != m_caster->GetInRangeSetEnd(); itr++ )
    {
        if((*itr)->IsUnit())
            if( (!(castPtr<Unit>(*itr)->isAlive())) || ( (*itr)->IsCreature() && castPtr<Creature>(*itr)->IsTotem() ))
                continue;

        if((*itr)->PhasedCanInteract(m_caster) == false)
            continue;

        if( GetSpellProto()->TargetCreatureType && (*itr)->IsUnit())
        {
            Unit* Target = castPtr<Unit>((*itr));
            if(!(1<<(Target->GetCreatureType()-1) & GetSpellProto()->TargetCreatureType))
                continue;
        }

        if( IsInrange( srcx, srcy, srcz, (*itr), r ))
        {
            if( u_caster != NULL )
            {
                if( sFactionSystem.isAttackable( u_caster, (*itr), !GetSpellProto()->isSpellStealthTargetCapable() ) )
                {
                    if((*itr)->IsUnit())
                        _AddTarget((castPtr<Unit>(*itr)), i);
                    else
                    {
                        _AddTargetForced((*itr)->GetGUID(), i);
                    }
                }
            }
            else //cast from GO
            {
                if( g_caster != NULL && g_caster->GetUInt64Value( GAMEOBJECT_FIELD_CREATED_BY ) && g_caster->m_summoner != NULL )
                {
                    //trap, check not to attack owner and friendly
                    if( sFactionSystem.isAttackable( g_caster->m_summoner, castPtr<Unit>(*itr), !GetSpellProto()->isSpellStealthTargetCapable() ) )
                        _AddTarget((castPtr<Unit>(*itr)), i);
                }
                else
                    _AddTargetForced((*itr)->GetGUID(), i);
            }
            if( GetSpellProto()->MaxTargets )
                if( m_hitTargetCount >= GetSpellProto()->MaxTargets )
                    return;
        }
    }
}

/// We fill all the gameobject targets in the area
void Spell::FillAllGameObjectTargetsInArea(uint32 i,float srcx,float srcy,float srcz, float range)
{
    float r = range*range;

    for(std::unordered_set<WorldObject* >::iterator itr = m_caster->GetInRangeSetBegin(); itr != m_caster->GetInRangeSetEnd(); itr++ )
    {
        // don't add objects that are units and dead
        if(!(*itr)->IsGameObject())
            continue;
        GameObject * o = castPtr<GameObject>(*itr);

        if( IsInrange( srcx, srcy, srcz, o, r ))
        {
            _AddTargetForced(o, i);
        }
    }
}

uint64 Spell::GetSinglePossibleEnemy(uint32 i,float prange)
{
    uint8 reflect;
    float rMin = GetSpellProto()->Range[0], rMax = prange;
    if(rMax == 0.f)
    {
        rMax = GetSpellProto()->Range[2];
        if( GetSpellProto()->SpellGroupType && u_caster)
        {
            u_caster->SM_FFValue(SMT_RADIUS,&rMax,GetSpellProto()->SpellGroupType);
            u_caster->SM_PFValue(SMT_RADIUS,&rMax,GetSpellProto()->SpellGroupType);
        }
    }

    float srcx = m_caster->GetPositionX(), srcy = m_caster->GetPositionY(), srcz = m_caster->GetPositionZ();
    for( std::unordered_set<WorldObject* >::iterator itr = m_caster->GetInRangeSetBegin(); itr != m_caster->GetInRangeSetEnd(); itr++ )
    {
        if( !( (*itr)->IsUnit() ) || !castPtr<Unit>(*itr)->isAlive() || !(*itr)->PhasedCanInteract(m_caster))
            continue;

        if( GetSpellProto()->TargetCreatureType && (*itr)->IsUnit())
        {
            Unit* Target = castPtr<Unit>((*itr));
            if(!(1<<(Target->GetCreatureType()-1) & GetSpellProto()->TargetCreatureType))
                continue;
        }
        if(IsInrange(srcx,srcy,srcz,(*itr), rMax, rMin))
        {
            if( u_caster != NULL || (g_caster && g_caster->GetType() == GAMEOBJECT_TYPE_TRAP) )
            {
                if(sFactionSystem.isAttackable(u_caster, castPtr<Unit>(*itr),!GetSpellProto()->isSpellStealthTargetCapable()) && _DidHit(i, castPtr<Unit>(*itr), reflect)==SPELL_DID_HIT_SUCCESS)
                    return (*itr)->GetGUID(); 
            }
            else //cast from GO
            {
                if(g_caster && g_caster->GetUInt64Value(GAMEOBJECT_FIELD_CREATED_BY) && g_caster->m_summoner)
                {
                    //trap, check not to attack owner and friendly
                    if( sFactionSystem.isAttackable( g_caster->m_summoner, castPtr<Unit>(*itr),!GetSpellProto()->isSpellStealthTargetCapable() ) )
                        return (*itr)->GetGUID();
                }
            }
        }
    }
    return 0;
}

uint64 Spell::GetSinglePossibleFriend(uint32 i,float prange)
{
    uint8 reflect;
    float rMin = GetSpellProto()->Range[1], rMax = prange;
    if(rMax == 0.f)
    {
        rMax = GetSpellProto()->Range[3];
        if( GetSpellProto()->SpellGroupType && u_caster)
        {
            u_caster->SM_FFValue(SMT_RADIUS,&rMax,GetSpellProto()->SpellGroupType);
            u_caster->SM_PFValue(SMT_RADIUS,&rMax,GetSpellProto()->SpellGroupType);
        }
    }
    float srcx=m_caster->GetPositionX(),srcy=m_caster->GetPositionY(),srcz=m_caster->GetPositionZ();
    for(std::unordered_set<WorldObject* >::iterator itr = m_caster->GetInRangeSetBegin(); itr != m_caster->GetInRangeSetEnd(); itr++ )
    {
        if( !( (*itr)->IsUnit() ) || !castPtr<Unit>(*itr)->isAlive() )
            continue;
        if( GetSpellProto()->TargetCreatureType && (*itr)->IsUnit())
        {
            Unit* Target = castPtr<Unit>((*itr));
            if(!(1<<(Target->GetCreatureType()-1) & GetSpellProto()->TargetCreatureType))
                continue;
        }
        if(IsInrange(srcx,srcy,srcz,(*itr),rMax,rMin))
        {
            if( u_caster != NULL )
            {
                if( sFactionSystem.isFriendly( u_caster, castPtr<Unit>(*itr) ) && _DidHit(i, castPtr<Unit>(*itr), reflect)==SPELL_DID_HIT_SUCCESS)
                    return (*itr)->GetGUID(); 
            }
            else //cast from GO
            {
                if(g_caster && g_caster->GetUInt64Value(GAMEOBJECT_FIELD_CREATED_BY) && g_caster->m_summoner)
                {
                    //trap, check not to attack owner and friendly
                    if( sFactionSystem.isFriendly( g_caster->m_summoner, castPtr<Unit>(*itr) ) )
                        return (*itr)->GetGUID();
                }
            }
        }
    }
    return 0;
}

uint8 Spell::_DidHit(uint32 index, Unit* target, uint8 &reflectout)
{
    //note resistchance is vise versa, is full hit chance
    if( target == NULL )
        return SPELL_DID_HIT_MISS;

    /************************************************************************/
    /* Can't resist non-unit                                                */
    /************************************************************************/
    if(!u_caster)
        return SPELL_DID_HIT_SUCCESS;

    /************************************************************************/
    /* Can't reduce your own spells                                      */
    /************************************************************************/
    if(u_caster == target)
        return SPELL_DID_HIT_SUCCESS;

    /************************************************************************/
    /* Check if the unit is evading                                      */
    /************************************************************************/
    if(target->GetTypeId() == TYPEID_UNIT && ((Unit*)target)->GetAIInterface()->getAIState() == STATE_EVADE)
        return SPELL_DID_HIT_EVADE;

    /************************************************************************/
    /* Check if the target is immune to this spell school                  */
    /************************************************************************/
    if(target->SchoolImmunityList[GetSpellProto()->School])
        return SPELL_DID_HIT_IMMUNE;

    /*************************************************************************/
    /* Check if the target is immune to this mechanic                       */
    /*************************************************************************/
    if(target->GetMechanicDispels(GetSpellProto()->MechanicsType))
        return SPELL_DID_HIT_IMMUNE; // Moved here from Spell::CanCast

    /************************************************************************/
    /* Check if the target has a % resistance to this mechanic            */
    /************************************************************************/
    if( GetSpellProto()->MechanicsType < MECHANIC_COUNT)
    {
        float res = target->GetMechanicResistPCT(Spell::GetMechanic(m_spellInfo));
        if( !GetSpellProto()->isSpellResistanceIgnorant() && Rand(res))
            return SPELL_DID_HIT_RESIST;
    }

    /************************************************************************/
    /* Check if the spell is a melee attack and if it was missed/parried    */
    /************************************************************************/
    if( GetSpellProto()->IsSpellWeaponSpell() )
    {
        uint32 _type = MELEE;
        if( GetType() == SPELL_DMG_TYPE_RANGED )
            _type = RANGED;
        else if (GetSpellProto()->reqOffHandWeapon())
            _type = OFFHAND;

        if(uint32 melee_test_result = u_caster->GetSpellDidHitResult( (Unit*)target, _type, m_spellInfo ))
            return uint8(melee_test_result & 0xFF);
    }

    return u_caster->GetSpellDidHitResult(index, (Unit*)target, this, reflectout);
}

bool Spell::GenerateTargets(SpellCastTargets *t)
{
    if(u_caster == NULL || u_caster->GetAIInterface() == NULL || !u_caster->IsInWorld())
        return false;

    bool result = false;

    for(uint32 i = 0; i < 3; ++i)
    {
        if(m_spellInfo->Effect[i] == 0)
            continue;
        uint32 TargetType = GetTargetType(m_spellInfo->EffectImplicitTargetA[i], i);

        //never get info from B if it is 0 :P
        if(m_spellInfo->EffectImplicitTargetB[i] != 0)
            TargetType |= GetTargetType(m_spellInfo->EffectImplicitTargetB[i], i);

        if(TargetType & (SPELL_TARGET_OBJECT_SELF | SPELL_TARGET_AREA_PARTY | SPELL_TARGET_AREA_RAID))
        {
            t->m_targetMask |= TARGET_FLAG_UNIT;
            t->m_unitTarget = u_caster->GetGUID();
            result = true;
        }

        if(TargetType & SPELL_TARGET_NO_OBJECT)
        {
            t->m_targetMask = TARGET_FLAG_SELF;
            t->m_unitTarget = u_caster->GetGUID();
            result = true;
        }

        if(!(TargetType & (SPELL_TARGET_AREA | SPELL_TARGET_AREA_SELF | SPELL_TARGET_AREA_CURTARGET | SPELL_TARGET_AREA_CONE)))
        {
            if(TargetType & SPELL_TARGET_ANY_OBJECT)
            {
                if(u_caster->GetUInt64Value(UNIT_FIELD_TARGET))
                {
                    //generate targets for things like arcane missiles trigger, tame pet, etc
                    WorldObject* target = u_caster->GetMapMgr()->_GetObject(u_caster->GetUInt64Value(UNIT_FIELD_TARGET));
                    if(target != NULL)
                    {
                        if(target->IsUnit())
                        {
                            t->m_targetMask |= TARGET_FLAG_UNIT;
                            t->m_unitTarget = target->GetGUID();
                            result = true;
                        }
                        else if(target->IsGameObject())
                        {
                            t->m_targetMask |= TARGET_FLAG_OBJECT;
                            t->m_unitTarget = target->GetGUID();
                            result = true;
                        }
                    }
                    result = true;
                }
            }

            if(TargetType & SPELL_TARGET_REQUIRE_ATTACKABLE)
            {
                if(u_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT))
                {
                    //generate targets for things like arcane missiles trigger, tame pet, etc
                    WorldObject* target = u_caster->GetMapMgr()->_GetObject(u_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT));
                    if(target != NULL)
                    {
                        if(target->IsUnit())
                        {
                            t->m_targetMask |= TARGET_FLAG_UNIT;
                            t->m_unitTarget = target->GetGUID();
                            result = true;
                        }
                        else if(target->IsGameObject())
                        {
                            t->m_targetMask |= TARGET_FLAG_OBJECT;
                            t->m_unitTarget = target->GetGUID();
                            result = true;
                        }
                    }
                }
                else if(u_caster->GetUInt64Value(UNIT_FIELD_TARGET))
                {
                    //generate targets for things like arcane missiles trigger, tame pet, etc
                    WorldObject* target = u_caster->GetMapMgr()->_GetObject(u_caster->GetUInt64Value(UNIT_FIELD_TARGET));
                    if(target != NULL)
                    {
                        if(target->IsUnit())
                        {
                            t->m_targetMask |= TARGET_FLAG_UNIT;
                            t->m_unitTarget = target->GetGUID();
                            result = true;
                        }
                        else if(target->IsGameObject())
                        {
                            t->m_targetMask |= TARGET_FLAG_OBJECT;
                            t->m_unitTarget = target->GetGUID();
                            result = true;
                        }
                    }
                    result = true;
                }
                else if(u_caster->IsCreature() && u_caster->IsTotem())
                {
                    Unit* target = u_caster->GetMapMgr()->GetUnit(GetSinglePossibleEnemy(i));
                    if(target != NULL)
                    {
                        t->m_targetMask |= TARGET_FLAG_UNIT;
                        t->m_unitTarget = target->GetGUID();
                    }
                }
            }

            if(TargetType & SPELL_TARGET_REQUIRE_FRIENDLY)
            {
                Unit* target = u_caster->GetMapMgr()->GetUnit(GetSinglePossibleFriend(i));
                if(target != NULL)
                {
                    t->m_targetMask |= TARGET_FLAG_UNIT;
                    t->m_unitTarget = target->GetGUID();
                    result = true;
                }
                else
                {
                    t->m_targetMask |= TARGET_FLAG_UNIT;
                    t->m_unitTarget = u_caster->GetGUID();
                    result = true;
                }
            }
        }

        if(TargetType & SPELL_TARGET_AREA_RANDOM)
        {
            //we always use radius(0) for some reason
            uint8 attempts = 0;
            do
            {
                //prevent deadlock
                ++attempts;
                if(attempts > 10)
                    return false;

                float r = RandomFloat(GetRadius(0));
                float ang = RandomFloat(M_PI * 2);
                t->m_destX = m_caster->GetPositionX() + (cosf(ang) * r);
                t->m_destY = m_caster->GetPositionY() + (sinf(ang) * r);
                t->m_destZ = m_caster->GetCHeightForPosition(true, t->m_destX, t->m_destY, m_caster->GetPositionZ() + 2.0f);
                t->m_targetMask = TARGET_FLAG_DEST_LOCATION;
            }
            while(sWorld.Collision && !sVMapInterface.CheckLOS(m_caster->GetMapId(), m_caster->GetInstanceID(), m_caster->GetPhaseMask(), m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ(), t->m_destX, t->m_destY, t->m_destZ));
            result = true;
        }
        else if(TargetType & SPELL_TARGET_AREA)  //targetted aoe
        {
            if(u_caster->GetAIInterface()->GetNextTarget() != NULL && TargetType & SPELL_TARGET_REQUIRE_ATTACKABLE)
            {
                t->m_targetMask |= TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_UNIT;
                t->m_unitTarget = u_caster->GetAIInterface()->GetNextTarget()->GetGUID();
                t->m_destX = u_caster->GetAIInterface()->GetNextTarget()->GetPositionX();
                t->m_destY = u_caster->GetAIInterface()->GetNextTarget()->GetPositionY();
                t->m_destZ = u_caster->GetAIInterface()->GetNextTarget()->GetPositionZ();
                result = true;
            }

            if(TargetType & SPELL_TARGET_REQUIRE_FRIENDLY)
            {
                t->m_targetMask |= TARGET_FLAG_DEST_LOCATION;
                t->m_destX = u_caster->GetPositionX();
                t->m_destY = u_caster->GetPositionY();
                t->m_destZ = u_caster->GetPositionZ();
                result = true;
            }
            else if(u_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT)) //spells like blizzard, rain of fire
            {
                WorldObject* target = u_caster->GetMapMgr()->_GetObject(u_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT));
                if(target != NULL)
                {
                    t->m_targetMask |= TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_UNIT;
                    t->m_unitTarget = target->GetGUID();
                    t->m_destX = target->GetPositionX();
                    t->m_destY = target->GetPositionY();
                    t->m_destZ = target->GetPositionZ();
                }
                result = true;
            }
        }
        else if(TargetType & SPELL_TARGET_AREA_SELF)
        {
            t->m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
            t->m_destX = t->m_srcX = u_caster->GetPositionX();
            t->m_destY = t->m_srcY = u_caster->GetPositionY();
            t->m_destZ = t->m_srcZ = u_caster->GetPositionZ();
            result = true;
        }

        if(TargetType & SPELL_TARGET_AREA_CHAIN)
        {
            if(TargetType & SPELL_TARGET_REQUIRE_ATTACKABLE)
            {
                if(u_caster->GetAIInterface()->GetNextTarget() != NULL)
                {
                    t->m_targetMask |= TARGET_FLAG_UNIT;
                    t->m_unitTarget = u_caster->GetAIInterface()->GetNextTarget()->GetGUID();
                    result = true;
                }
            }
            else
            {
                t->m_targetMask |= TARGET_FLAG_UNIT;
                t->m_unitTarget = u_caster->GetGUID();
                result = true;
            }
        }

        //target cone
        if(TargetType & SPELL_TARGET_AREA_CONE)
        {
            if(u_caster->GetAIInterface()->GetNextTarget() != NULL)
            {
                t->m_targetMask |= TARGET_FLAG_DEST_LOCATION;
                t->m_destX = u_caster->GetAIInterface()->GetNextTarget()->GetPositionX();
                t->m_destY = u_caster->GetAIInterface()->GetNextTarget()->GetPositionY();
                t->m_destZ = u_caster->GetAIInterface()->GetNextTarget()->GetPositionZ();
                result = true;
            }
        }
    }
    return result;
}

uint8 Spell::prepare( SpellCastTargets * targets )
{
    uint8 ccr;
    if( p_caster && (GetSpellProto()->Id == 51514 || GetSpellProto()->NameHash == SPELL_HASH_ARCANE_SHOT || GetSpellProto()->NameHash == SPELL_HASH_MIND_FLAY))
    {
        targets->m_unitTarget = 0;
        GenerateTargets( targets );
    }

    m_targets = *targets;
    m_missileTravelTime = floor(m_targets.traveltime);
    m_missilePitch = m_targets.missilepitch;

    if(m_missileTravelTime || m_spellInfo->speed > 0.0f && !m_spellInfo->IsSpellChannelSpell() || m_spellInfo->Id == 14157)
        m_projectileWait = true;

    if( !m_triggeredSpell && p_caster != NULL && p_caster->CastTimeCheat )
        m_castTime = 0;
    else
    {
        m_castTime = m_ForcedCastTime ? m_ForcedCastTime : GetDBCCastTime( dbcSpellCastTime.LookupEntry( GetSpellProto()->CastingTimeIndex ) );
        if( m_castTime && GetSpellProto()->SpellGroupType && u_caster != NULL )
        {
            u_caster->SM_FIValue( SMT_CAST_TIME, (int32*)&m_castTime, GetSpellProto()->SpellGroupType );
            u_caster->SM_PIValue( SMT_CAST_TIME, (int32*)&m_castTime, GetSpellProto()->SpellGroupType );
        }

        // handle MOD_CAST_TIME
        if( u_caster != NULL && m_castTime && !m_ForcedCastTime)
            m_castTime = float2int32( m_castTime * u_caster->GetFloatValue( UNIT_MOD_CAST_SPEED ) );
    }

    uint8 forced_cancast_failure = 0;
    if( u_caster != NULL )
    {
        if( GetGameObjectTarget() || GetSpellProto()->Id == 21651)
        {
            if( u_caster->InStealth() )
                u_caster->RemoveAura( u_caster->m_stealth );

            if( (GetSpellProto()->Effect[0] == SPELL_EFFECT_OPEN_LOCK ||
                GetSpellProto()->Effect[1] == SPELL_EFFECT_OPEN_LOCK ||
                GetSpellProto()->Effect[2] == SPELL_EFFECT_OPEN_LOCK) &&
                p_caster != NULL && p_caster->m_bgFlagIneligible)
                forced_cancast_failure = SPELL_FAILED_BAD_TARGETS;
        }
    }

    //let us make sure cast_time is within decent range
    //this is a hax but there is no spell that has more then 10 minutes cast time
    if( m_castTime < 0 )
        m_castTime = 0;
    else if( m_castTime > 60 * 10 * 1000)
        m_castTime = 60 * 10 * 1000; //we should limit cast time to 10 minutes right ?

    m_timer = m_castTime;

    if( !m_triggeredSpell && p_caster != NULL && p_caster->CooldownCheat )
        p_caster->ClearCooldownForSpell( GetSpellProto()->Id );

    if(objmgr.IsSpellDisabled(GetSpellProto()->Id))//if it's disabled it will not be casted, even if it's triggered.
        cancastresult = uint8(m_triggeredSpell ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_SPELL_UNAVAILABLE);
    else if( m_triggeredSpell || ProcedOnSpell != NULL)
        cancastresult = SPELL_CANCAST_OK;
    else
        cancastresult = CanCast(false);

    ccr = cancastresult;

    if( forced_cancast_failure )
        cancastresult = forced_cancast_failure;

    if( cancastresult != SPELL_CANCAST_OK )
    {
        SendCastResult( cancastresult );

        if( m_triggeredByAura )
        {
            SendChannelUpdate( 0 );
            if( u_caster != NULL )
                u_caster->RemoveAura( m_triggeredByAura );
        }
        else
        {
            // HACK, real problem is the way spells are handled
            // when a spell is channeling and a new spell is casted
            // that is a channeling spell, but not triggert by a aura
            // the channel bar/spell is bugged
            if( u_caster && u_caster->GetUInt64Value( UNIT_FIELD_CHANNEL_OBJECT) > 0 && u_caster->GetCurrentSpell() )
            {
                u_caster->GetCurrentSpell()->cancel();
                SendChannelUpdate( 0 );
                cancel();
                return ccr;
            }
        }
        finish();
        return ccr;
    }

    if( m_triggeredSpell && (!GetSpellProto()->IsSpellChannelSpell() || !GetSpellInfoDuration(GetSpellProto(), u_caster, NULL)))
    {
        cast( false );
        return ccr;
    }

    if( !HasPower() )
    {
        SendCastResult(SPELL_FAILED_NO_POWER);
        // in case we're out of sync
        if( p_caster )
            u_caster->SendPowerUpdate();

        return SPELL_FAILED_NO_POWER;
    }

    SendSpellStart();

    // start cooldown handler
    if( p_caster != NULL && !p_caster->CastTimeCheat )
        AddStartCooldown();

    if( i_caster == NULL )
    {
        if( p_caster != NULL && m_timer > 0 )
            p_caster->delayAttackTimer( m_timer + 1000 );
    }

    // aura state removal
    if( GetSpellProto()->CasterAuraState && GetSpellProto()->CasterAuraState != AURASTATE_FLAG_JUDGEMENT )
        u_caster->RemoveFlag( UNIT_FIELD_AURASTATE, GetSpellProto()->CasterAuraState );

    m_spellState = SPELL_STATE_PREPARING;

    // instant cast(or triggered) and not channeling
    if( u_caster != NULL && ( m_castTime > 0 || GetSpellProto()->IsSpellChannelSpell() ) && !m_triggeredSpell )
    {
        m_castPositionX = m_caster->GetPositionX();
        m_castPositionY = m_caster->GetPositionY();
        m_castPositionZ = m_caster->GetPositionZ();
        u_caster->CastSpell( this );
    }
    else
        cast( false );

    return ccr;
}

void Spell::cancel()
{
    if(m_spellState == SPELL_STATE_FINISHED)
        return;

    SendInterrupted(0);
    SendCastResult(SPELL_FAILED_INTERRUPTED);

    if(m_spellState == SPELL_STATE_CASTING)
    {
        if( u_caster != NULL )
            u_caster->RemoveAura(GetSpellProto()->Id);

        if(m_timer > 0 || m_Delayed)
        {
            if(p_caster && p_caster->IsInWorld())
            {
                Unit* pTarget = p_caster->GetMapMgr()->GetUnit(m_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT));
                if(!pTarget)
                    pTarget = p_caster->GetMapMgr()->GetUnit(p_caster->GetSelection());

                if(pTarget)
                    pTarget->RemoveAura(GetSpellProto()->Id, m_caster->GetGUID());

                if(m_AreaAura)//remove of blizz and shit like this
                {
                    DynamicObject* dynObj = m_caster->GetMapMgr()->GetDynamicObject(m_caster->GetUInt32Value(UNIT_FIELD_CHANNEL_OBJECT));
                    if(dynObj)
                    {
                        dynObj->RemoveFromWorld(true);
                        dynObj->Destruct();
                        dynObj = NULL;
                    }
                }

                if(p_caster && p_caster->GetSummonedObject())
                {
                    if(p_caster->GetSummonedObject()->IsInWorld())
                        p_caster->GetSummonedObject()->RemoveFromWorld(true);
                    // for now..
                    ASSERT(p_caster->GetSummonedObject()->GetTypeId() == TYPEID_GAMEOBJECT);
                    castPtr<GameObject>(p_caster->GetSummonedObject())->Destruct();
                    p_caster->SetSummonedObject(NULL);
                }

                if(m_timer > 0)
                {
                    p_caster->delayAttackTimer(-m_timer);
                    RemoveItems();
                }
             }
        }
    }

    SendChannelUpdate(0);

    // Ensure the item gets consumed once the channel has started
    if (m_timer > 0)
        m_ForceConsumption = true;

    if( !m_isCasting )
        finish();
}

void Spell::AddCooldown()
{
    if( p_caster != NULL && !p_caster->CooldownCheat)
        p_caster->Cooldown_Add( m_spellInfo, i_caster );
}

void Spell::AddStartCooldown()
{
    if( p_caster != NULL && !p_caster->CastTimeCheat)
        p_caster->Cooldown_AddStart( m_spellInfo );
}

void Spell::cast(bool check)
{
    if( duelSpell && (
        ( p_caster != NULL && p_caster->GetDuelState() != DUEL_STATE_STARTED ) ||
        ( u_caster != NULL && u_caster->IsPet() && castPtr<Pet>( u_caster )->GetPetOwner() && castPtr<Pet>( u_caster )->GetPetOwner()->GetDuelState() != DUEL_STATE_STARTED ) ) )
    {
        // Can't cast that!
        SendInterrupted( SPELL_FAILED_TARGET_FRIENDLY );
        finish();
        return;
    }

    sLog.Debug("Spell","Cast %u, Unit: %u", GetSpellProto()->Id, m_caster->GetLowGUID());

    // Set the base ms time to now
    MSTimeToAddToTravel = getMSTime();

    if(objmgr.IsSpellDisabled(GetSpellProto()->Id))//if it's disabled it will not be casted, even if it's triggered.
        cancastresult = uint8(m_triggeredSpell ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_SPELL_UNAVAILABLE);
    else if(check)
        cancastresult = CanCast(true);
    else
        cancastresult = SPELL_CANCAST_OK;

    if(cancastresult == SPELL_CANCAST_OK)
    {
        if (p_caster && !m_triggeredSpell && p_caster->IsInWorld() && GUID_HIPART(m_targets.m_unitTarget) == HIGHGUID_TYPE_CREATURE)
            sQuestMgr.OnPlayerCast(p_caster, GetSpellProto()->Id, m_targets.m_unitTarget);

        // trigger on next attack
        if( GetSpellProto()->isNextMeleeAttack1() )
        {
            if(!m_triggeredSpell)
            {
                // check power
                if(!HasPower())
                {
                    SendInterrupted(SPELL_FAILED_NO_POWER);
                    SendCastResult(SPELL_FAILED_NO_POWER);
                    finish();
                    return;
                }
            }
            else if(!TakePower()) // this is the actual spell cast
            {
                SendInterrupted(SPELL_FAILED_NO_POWER);
                SendCastResult(SPELL_FAILED_NO_POWER);
                finish();
                return;
            }
        }
        else if(!m_triggeredSpell)
        {
            if(!TakePower())
            {
                SendInterrupted(SPELL_FAILED_NO_POWER);
                SendCastResult(SPELL_FAILED_NO_POWER);
                finish();
                return;
            }
        }

        for(uint32 i = 0; i < 3; i++)
        {
            uint32 TargetType = 0;

            // Fill from A regardless
            TargetType |= GetTargetType(m_spellInfo->EffectImplicitTargetA[i], i);

            //never get info from B if it is 0 :P
            if(m_spellInfo->EffectImplicitTargetB[i] != 0)
                TargetType |= GetTargetType(m_spellInfo->EffectImplicitTargetB[i], i);

            if(TargetType & SPELL_TARGET_AREA_CURTARGET)
            {
                //this just forces dest as the targets location :P
                WorldObject* target = m_caster->GetMapMgr()->_GetObject(m_targets.m_unitTarget);
                if(target != NULL)
                {
                    m_targets.m_targetMask = TARGET_FLAG_DEST_LOCATION;
                    m_targets.m_destX = target->GetPositionX();
                    m_targets.m_destY = target->GetPositionY();
                    m_targets.m_destZ = target->GetPositionZ();
                    break;
                }
            }
        }

        SendCastResult(cancastresult);
        if(cancastresult != SPELL_CANCAST_OK)
        {
            finish();
            return;
        }

        m_isCasting = true;

        if(!m_triggeredSpell)
            AddCooldown();

        if( p_caster != NULL )
        {
            if(p_caster->InStealth() && !m_spellInfo->isStealthUnaffecting() && m_spellInfo->Id != 1)  //check spells that get trigger spell 1 after spell loading
            {
                /* talents procing - don't remove stealth either */
                if(!m_spellInfo->isPassiveSpell() && !(m_triggeredSpellId && dbcSpell.LookupEntry(m_triggeredSpellId)->isPassiveSpell()))
                {
                    p_caster->RemoveAura(p_caster->m_stealth);
                    p_caster->m_stealth = 0;
                }
            }

            // Arathi Basin opening spell, remove stealth, invisibility, etc.
            // hacky but haven't found a better way that works
            // Note: Same stuff but for picking flags is over AddAura
            if (p_caster->m_bg)
            {
                // SOTA Gameobject spells
                if(p_caster->m_bg->GetType() == BATTLEGROUND_STRAND_OF_THE_ANCIENTS)
                {
                    StrandOfTheAncients* sota = (StrandOfTheAncients*)p_caster->m_bg;
                    // Transporter platforms
                    if(m_spellInfo->Id == 54640)
                        sota->OnPlatformTeleport(p_caster);
                }

                // warsong gulch & eye of the storm flag pickup check
                // also includes check for trying to cast stealth/etc while you have the flag
                switch(m_spellInfo->Id)
                {
                case 21651:
                    // Arathi Basin opening spell, remove stealth, invisibility, etc.
                    p_caster->RemoveStealth();
                    p_caster->RemoveInvisibility();
                    p_caster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_DIVINE_SHIELD, false);
                    p_caster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_DIVINE_PROTECTION, false);
                    p_caster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_BLESSING_OF_PROTECTION, false);
                    break;
                case 23333:
                case 23335:
                case 34976:
                    // if we're picking up the flag remove the buffs
                    p_caster->RemoveStealth();
                    p_caster->RemoveInvisibility();
                    p_caster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_DIVINE_SHIELD, false);
                    p_caster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_DIVINE_PROTECTION, false);
                    p_caster->m_AuraInterface.RemoveAllAurasByNameHash(SPELL_HASH_BLESSING_OF_PROTECTION, false);
                    break;
                    // cases for stealth - etc
                    // we can cast the spell, but we drop the flag (if we have it)
                case 1784:      // Stealth rank 1
                case 1785:      // Stealth rank 2
                case 1786:      // Stealth rank 3
                case 1787:      // Stealth rank 4
                case 5215:      // Prowl rank 1
                case 6783:      // Prowl rank 2
                case 9913:      // Prowl rank 3
                case 498:       // Divine protection
                case 5573:      // Unknown spell
                case 642:       // Divine shield
                case 1020:      // Unknown spell
                case 1022:      // Hand of Protection rank 1 (ex blessing of protection)
                case 5599:      // Hand of Protection rank 2 (ex blessing of protection)
                case 10278:     // Hand of Protection rank 3 (ex blessing of protection)
                case 1856:      // Vanish rank 1
                case 1857:      // Vanish rank 2
                case 26889:     // Vanish rank 3
                case 45438:     // Ice block
                case 20580:     // Unknown spell
                case 58984:     // Shadowmeld
                case 17624:     // Petrification-> http://www.wowhead.com/?spell=17624
                case 66:        // Invisibility
                    {
                        if(p_caster->m_bg->GetType() == BATTLEGROUND_WARSONG_GULCH)
                        {
                            if(p_caster->GetTeam() == 0)
                                p_caster->RemoveAura(23333);    // ally player drop horde flag if they have it
                            else
                                p_caster->RemoveAura(23335);    // horde player drop ally flag if they have it
                        }
                        if(p_caster->m_bg->GetType() == BATTLEGROUND_EYE_OF_THE_STORM)
                            p_caster->RemoveAura(34976);    // drop the flag
                    }break;
                }
            }
        }

        if(!m_spellInfo->isNextMeleeAttack1() || m_triggeredSpell)  //on next attack
        {
            SendSpellGo();

            //******************** SHOOT SPELLS ***********************
            if(GetSpellProto()->isSpellRangedSpell() && (p_caster != NULL) && m_caster->IsInWorld())
            {
                /// Part of this function contains a hack fix
                /// hack fix for shoot spells, should be some other resource for it
                //p_caster->SendSpellCoolDown(GetSpellProto()->Id, GetSpellProto()->RecoveryTime ? GetSpellProto()->RecoveryTime : 2300);
                WorldPacket data(SMSG_SPELL_COOLDOWN, 14);
                data << p_caster->GetGUID();
                data << uint8(0);
                data << GetSpellProto()->Id;
                data << uint32(GetSpellProto()->RecoveryTime ? GetSpellProto()->RecoveryTime : 2300);
                p_caster->GetSession()->SendPacket(&data);
            }
            else if( GetSpellProto()->IsSpellChannelSpell() && !m_triggeredSpell && GetSpellProto()->Id != 50589 ) // Note: [Warlock] Immolation Aura somehow has these flags, but it's not channeled
            {
                /*
                Channeled spells are handled a little differently. The five second rule starts when the spell's channeling starts; i.e. when you pay the mana for it.
                The rule continues for at least five seconds, and longer if the spell is channeled for more than five seconds. For example,
                Mind Flay channels for 3 seconds and interrupts your regeneration for 5 seconds, while Tranquility channels for 10 seconds
                and interrupts your regeneration for the full 10 seconds.
                */
                m_spellState = SPELL_STATE_CASTING;
                SendChannelStart(GetDuration());
                if( p_caster != NULL && p_caster->GetSelection() )
                {
                    //Use channel interrupt flags here
                    if(m_targets.m_targetMask == TARGET_FLAG_DEST_LOCATION || m_targets.m_targetMask == TARGET_FLAG_SOURCE_LOCATION)
                        u_caster->SetChannelSpellTargetGUID(p_caster->GetSelection());
                    else if(p_caster->GetSelection() == m_caster->GetGUID())
                    {
                        if(p_caster->GetSummon())
                            u_caster->SetChannelSpellTargetGUID(p_caster->GetSummon()->GetGUID());
                        else if(m_targets.m_unitTarget)
                            u_caster->SetChannelSpellTargetGUID(m_targets.m_unitTarget);
                        else u_caster->SetChannelSpellTargetGUID(p_caster->GetSelection());
                    }
                    else
                    {
                        if(p_caster->GetSelection())
                            u_caster->SetChannelSpellTargetGUID(p_caster->GetSelection());
                        else if(p_caster->GetSummon())
                            u_caster->SetChannelSpellTargetGUID(p_caster->GetSummon()->GetGUID());
                        else if(m_targets.m_unitTarget)
                            u_caster->SetChannelSpellTargetGUID(m_targets.m_unitTarget);
                        else
                        {
                            m_isCasting = false;
                            cancel();
                            return;
                        }
                    }
                }
            }

            if(!m_projectileWait)
            {
                // if the spell is not reflected
                bool effects_done[3] = { false, false, false };
                for(uint8 i = 0; i < 3; i++)
                {
                    if(GetSpellProto()->Effect[i])
                    {
                        FillTargetMap(i);
                        if(ManagedTargets.size())
                        {
                            for(SpellTargetMap::iterator itr = ManagedTargets.begin(); itr != ManagedTargets.end(); itr++)
                            {
                                if( itr->second.HitResult == SPELL_DID_HIT_SUCCESS )
                                {
                                    // set target pointers
                                    _SetTargets(itr->first);

                                    // call effect handlers
                                    if(CanHandleSpellEffect(i, m_spellInfo->NameHash))
                                    {
                                        //Don't handle effect now
                                        if(GetSpellProto()->Effect[i] != SPELL_EFFECT_SUMMON)
                                        {
                                            HandleEffects(i);
                                            effects_done[i] = true;
                                        }
                                        else
                                            effects_done[i] = false;
                                    }
                                }
                            }

                            for(SpellTargetMap::iterator itr = ManagedTargets.begin(); itr != ManagedTargets.end(); itr++)
                            {
                                if( itr->second.HitResult == SPELL_DID_HIT_SUCCESS )
                                {
                                    // set target pointers
                                    _SetTargets(itr->first);

                                    // handle the rest of shit
                                    if( unitTarget != NULL )
                                    {
                                        // aura state
                                        if( GetSpellProto()->TargetAuraState )
                                            unitTarget->RemoveFlag(UNIT_FIELD_AURASTATE, uint32(1) << (GetSpellProto()->TargetAuraState - 1) );
                                    }
                                }
                            }
                            ManagedTargets.clear();
                        }
                        else
                        {
                            // set target pointers
                            _SetTargets(0);

                            // call effect handlers
                            if(GetSpellProto()->Effect[i])
                            {
                                if(CanHandleSpellEffect(i, m_spellInfo->NameHash))
                                {
                                    //Don't handle effect now
                                    if(GetSpellProto()->Effect[i] != SPELL_EFFECT_SUMMON)
                                    {
                                        HandleEffects(i);
                                        effects_done[i] = true;
                                    }
                                    else
                                        effects_done[i] = false;
                                }
                            }

                            // handle the rest of shit
                            if( unitTarget != NULL )
                            {
                                // aura state
                                if( GetSpellProto()->TargetAuraState )
                                    unitTarget->RemoveFlag(UNIT_FIELD_AURASTATE, uint32(1) << (GetSpellProto()->TargetAuraState - 1) );
                            }
                        }
                    }
                }

                //Handle remaining effects for which we did not find targets.
                for( uint32 x = 0; x < 3; ++x )
                {
                    if(GetSpellProto()->Effect[x])
                    {
                        if(!CanHandleSpellEffect(x, m_spellInfo->NameHash))
                            continue;

                        if(!effects_done[x])
                        {
                            switch (GetSpellProto()->Effect[x])
                            {
                                // Target ourself for these effects
                            case SPELL_EFFECT_TRIGGER_SPELL:
                            case SPELL_EFFECT_SUMMON:
                                {
                                    _SetTargets(m_caster->GetGUID());
                                    HandleEffects(x);
                                }break;

                                // No Target required for these effects
                            case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                                {
                                    HandleEffects(x);
                                }break;
                            }
                        }
                    }
                }

                /* don't call HandleAddAura unless we actually have auras... - Burlex */
                if( GetSpellProto()->EffectApplyAuraName[0] != 0 ||
                    GetSpellProto()->EffectApplyAuraName[1] != 0 ||
                    GetSpellProto()->EffectApplyAuraName[2] != 0 )
                {
                    for(SpellTargetMap::iterator itr = TargetMap.begin(); itr != TargetMap.end(); itr++)
                    {
                        if( itr->second.HitResult != SPELL_DID_HIT_SUCCESS )
                            continue;

                        HandleAddAura(itr->first);
                    }
                }
            }
            else
                CalcDestLocationHit();

            // we're much better to remove this here, because otherwise spells that change powers etc,
            // don't get applied.
            if( u_caster )
                u_caster->m_AuraInterface.RemoveAllAurasByInterruptFlagButSkip(AURA_INTERRUPT_ON_CAST_SPELL, GetSpellProto()->Id);

            m_isCasting = false;

            if(m_spellState != SPELL_STATE_CASTING)
                finish();
        }
        else //this shit has nothing to do with instant, this only means it will be on NEXT melee hit
        {
            // we're much better to remove this here, because otherwise spells that change powers etc,
            // don't get applied.
            if(u_caster && !m_triggeredSpell && !m_triggeredByAura)
                u_caster->m_AuraInterface.RemoveAllAurasByInterruptFlagButSkip(AURA_INTERRUPT_ON_CAST_SPELL, GetSpellProto()->Id);

            m_isCasting = false;
            SendCastResult(cancastresult);
            if(u_caster != NULL)
                u_caster->SetOnMeleeSpell(GetSpellProto()->Id, extra_cast_number);

            finish();
            return;
        }
    }
    else
    {
        // cancast failed
        SendCastResult(cancastresult);
        SendInterrupted(cancastresult);
        finish();
    }
}

void Spell::CalcDestLocationHit()
{
    bool hit = false;
    for(uint32 x = 0; x < 3; ++x)
    {
        FillTargetMap(x);
        ManagedTargets.clear();
    }

    if(TargetMap.size())
    {
        std::set<WoWGuid> toHit;
        for(SpellTargetMap::iterator itr = TargetMap.begin(); itr != TargetMap.end(); itr++)
        {
            if( itr->second.HitResult == SPELL_DID_HIT_SUCCESS )
            {
                // set target pointers
                _SetTargets(itr->first);
                if(unitTarget == NULL)
                    continue;

                float dist = m_caster->CalcDistance(unitTarget);
                if (dist < 5.0f)
                    dist = 5.0f;
                if(dist == 5.0f)
                    toHit.insert(itr->first);
                else
                {
                    unitTarget->AddDelayedSpell(this);
                    hit = true;
                }
            }
        }

        if(toHit.size())
        {
            for(std::set<WoWGuid>::iterator itr = toHit.begin(); itr != toHit.end(); itr++)
                HandleDestTargetHit(*itr, 0xFFFFFFFF);
        }
    }
    else
    {
        _SetTargets(0);
        if(unitTarget != NULL)
        {
            unitTarget->AddDelayedSpell(this);
            hit = true;
        }
    }
    m_projectileWait = hit;
}

void Spell::HandleRemoveDestTarget(uint64 guid)
{
    TargetMap.erase(guid);
    if(!TargetMap.size())
        Destruct();
}

bool Spell::HandleDestTargetHit(uint64 guid, uint32 MSTime)
{
    SpellTargetMap::iterator itr = TargetMap.find(guid);
    if(itr != TargetMap.end())
    {
        if(itr->second.DestinationTime > MSTime)
        {
            itr->second.DestinationTime -= MSTime;
//          SendProjectileUpdate();
            return false;
        }
        else
        {
            _SetTargets(itr->first);
            if(unitTarget == NULL || (unitTarget && unitTarget->isAlive()))
            {
                // if the spell is not reflected
                bool effects_done[3] = { false, false, false };
                for(uint32 i = 0; i < 3; i++)
                {
                    if(GetSpellProto()->Effect[i])
                    {
                        // call effect handlers
                        if(CanHandleSpellEffect(i, m_spellInfo->NameHash))
                        {
                            //Don't handle effect now
                            if(GetSpellProto()->Effect[i] != SPELL_EFFECT_SUMMON)
                            {
                                HandleEffects(i);
                                effects_done[i] = true;
                            }
                            else
                                effects_done[i] = false;
                        }
                    }
                }

                //Handle remaining effects for which we did not find targets.
                for( uint32 x = 0; x < 3; ++x )
                {
                    if(GetSpellProto()->Effect[x])
                    {
                        if(!CanHandleSpellEffect(x, m_spellInfo->NameHash))
                            continue;

                        if(!effects_done[x])
                        {
                            switch (GetSpellProto()->Effect[x])
                            {
                                // Target ourself for these effects
                            case SPELL_EFFECT_TRIGGER_SPELL:
                            case SPELL_EFFECT_SUMMON:
                                {
                                    _SetTargets(m_caster->GetGUID());
                                    HandleEffects(x);
                                }break;

                                // No Target required for these effects
                            case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                                {
                                    HandleEffects(x);
                                }break;
                            }
                        }
                    }
                }

                // handle the rest of shit
                if( unitTarget != NULL )
                {
                    // aura state
                    if( GetSpellProto()->TargetAuraState )
                        unitTarget->RemoveFlag(UNIT_FIELD_AURASTATE, uint32(1) << (GetSpellProto()->TargetAuraState - 1) );
                }

                /* don't call HandleAddAura unless we actually have auras... - Burlex */
                if( GetSpellProto()->EffectApplyAuraName[0] != 0 ||
                    GetSpellProto()->EffectApplyAuraName[1] != 0 ||
                    GetSpellProto()->EffectApplyAuraName[2] != 0 )
                {
                    HandleAddAura(itr->first);
                }
            }

            TargetMap.erase(itr);
        }
    }

    if(!TargetMap.size())
    {
        if(m_spellState == SPELL_STATE_FINISHED)
            Destruct();
        else m_projectileWait = false;
    }
    return true;
}

void Spell::AddTime(uint32 type)
{
    if(u_caster && u_caster->IsPlayer())
    {
        if( GetSpellProto()->InterruptFlags & CAST_INTERRUPT_ON_DAMAGE_TAKEN)
        {
            cancel();
            return;
        }

        if( GetSpellProto()->SpellGroupType && u_caster)
        {
            float ch = 0;
            u_caster->SM_FFValue(SMT_NONINTERRUPT, &ch, GetSpellProto()->SpellGroupType);
            if(Rand(ch))
                return;
        }

        if( p_caster != NULL )
        {
            if(Rand(p_caster->SpellDelayResist[type]))
                return;
        }

        if(m_pushbackCount > 1)
            return;

        m_pushbackCount++;
        if(m_spellState == SPELL_STATE_PREPARING)
        {
            int32 delay = 500;
            m_timer += delay;
            if(m_timer>m_castTime)
            {
                delay -= (m_timer - m_castTime);
                m_timer = m_castTime;
                if(delay < 0)
                    delay = 1;
            }

            WorldPacket data(SMSG_SPELL_DELAYED, 13);
            data << u_caster->GetGUID();
            data << uint32(delay);
            u_caster->SendMessageToSet(&data, true);

            if(!p_caster)
            {
                if(m_caster->GetTypeId() == TYPEID_UNIT)
                    u_caster->GetAIInterface()->AddStopTime(delay);
            }
            //in case cast is delayed, make sure we do not exit combat
            else
            {
//              sEventMgr.ModifyEventTimeLeft(p_caster,EVENT_ATTACK_TIMEOUT,PLAYER_ATTACK_TIMEOUT_INTERVAL,true);
                // also add a new delay to offhand and main hand attacks to avoid cutting the cast short
                p_caster->delayAttackTimer(delay);
            }
        }
        else if(GetSpellProto()->IsSpellChannelSpell())
        {
            int32 delay = GetDuration()/4;
            m_timer -= delay;
            if(m_timer < 0)
                m_timer = 0;
            else
                p_caster->delayAttackTimer(-delay);

            m_Delayed = true;
            if(m_timer > 0)
                SendChannelUpdate(m_timer);
        }
    }
}

void Spell::update(uint32 difftime)
{
    // ffs stealth capping
    if( p_caster && GetGameObjectTarget() && p_caster->InStealth() )
        p_caster->RemoveAura( p_caster->m_stealth );

    updatePosition(m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ());

    if(m_cancelled)
    {
        cancel();
        return;
    }

//  printf("spell::update m_timer %u, difftime %d, newtime %d\n", m_timer, difftime, m_timer-difftime);
    switch(m_spellState)
    {
    case SPELL_STATE_PREPARING:
        {
            if((int32)difftime >= m_timer)
            {
                m_timer = 0;
                cast(true);
            }
            else
                m_timer -= difftime;
        }break;
    case SPELL_STATE_CASTING:
        {
            if(m_timer > 0)
            {
                if((int32)difftime >= m_timer)
                    m_timer = 0;
                else
                    m_timer -= difftime;
            }

            if(m_timer <= 0)
            {
                SendChannelUpdate(0);
                finish();
            }
        }break;
    }
}

void Spell::updatePosition(float x, float y, float z)
{
    if((GetSpellProto()->InterruptFlags & CAST_INTERRUPT_ON_MOVEMENT) &&
        ( m_castPositionX != x || m_castPositionY != y || m_castPositionZ != z))
    {
        if(u_caster != NULL)
        {
            if(u_caster->HasNoInterrupt() == 0 && GetSpellProto()->EffectMechanic[1] != 14)
            {
                cancel();
                return;
            }
        }
    }

    if((m_spellInfo->IsSpellChannelSpell() && m_spellInfo->ChannelInterruptFlags & CHANNEL_INTERRUPT_ON_MOVEMENT) &&
        ( m_castPositionX != x || m_castPositionY != y || m_castPositionZ != z))
    {
        if(u_caster != NULL)
        {
            cancel();
            return;
        }
    }
}

void Spell::finish()
{
    if(m_spellState == SPELL_STATE_FINISHED)
        return;

    m_spellState = SPELL_STATE_FINISHED;
    if( u_caster != NULL )
        u_caster->m_canMove = true;

    //enable pvp when attacking another player with spells
    if( p_caster != NULL )
    {
        if (GetSpellProto()->isAttackInterrupting())
        {
            p_caster->EventAttackStop();
            p_caster->smsg_AttackStop( unitTarget );
        }

        if(GetSpellProto()->isComboPointReqSpell() && !GetSpellFailed())
        {
            if(p_caster->m_spellcomboPoints)
            {
                p_caster->m_comboPoints = p_caster->m_spellcomboPoints;
                p_caster->UpdateComboPoints(); //this will make sure we do not use any wrong values here
            } else p_caster->NullComboPoints();
        }
        if(m_Delayed && p_caster->IsInWorld())
        {
            Unit* pTarget = NULL;
            if((pTarget = p_caster->GetMapMgr()->GetUnit(m_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT))) == NULL)
                pTarget = p_caster->GetMapMgr()->GetUnit(p_caster->GetSelection());
            if(pTarget) pTarget->RemoveAura(GetSpellProto()->Id, m_caster->GetGUID());
        }
    }

    if( p_caster != NULL && GetSpellProto()->HasEffect(SPELL_EFFECT_SUMMON_OBJECT) )
        p_caster->SetSummonedObject(NULL);

    /*
    We set current spell only if this spell has cast time or is channeling spell
    otherwise it's instant spell and we delete it right after completion
    */
    if( u_caster != NULL )
    {
        if(!m_triggeredSpell && (GetSpellProto()->IsSpellChannelSpell() || m_castTime > 0))
            u_caster->SetCurrentSpell(NULL);
    }

    if( p_caster)
    {
        if(!GetSpellFailed())
            sHookInterface.OnPostSpellCast( p_caster, GetSpellProto(), unitTarget );

        if(p_caster->CooldownCheat && GetSpellProto())
            p_caster->ClearCooldownForSpell(GetSpellProto()->Id);

        if( m_ForceConsumption || ( cancastresult == SPELL_CANCAST_OK && !GetSpellFailed() ) )
            RemoveItems();
    }

    if(!m_projectileWait)
        Destruct();
}

void Spell::SendCastResult(uint8 result)
{
    if(result == SPELL_CANCAST_OK)
        return;

    SetSpellFailed();
    if(!m_caster->IsInWorld())
        return;

    Player* plr = p_caster;
    if( plr == NULL && u_caster != NULL)
        plr = u_caster->m_redirectSpellPackets;
    if( plr == NULL)
        return;

    // reset cooldowns
    if( m_spellState == SPELL_STATE_PREPARING )
        plr->Cooldown_OnCancel(m_spellInfo);

    uint32 Extra = 0;
    // for some reason, the result extra is not working for anything, including SPELL_FAILED_REQUIRES_SPELL_FOCUS
    switch( result )
    {
    case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
        Extra = GetSpellProto()->RequiresSpellFocus;
        break;

    case SPELL_FAILED_REQUIRES_AREA:
        {
            if( GetSpellProto()->AreaGroupId > 0 )
            {
                uint16 area_id = plr->GetAreaId();
                AreaGroupEntry *GroupEntry = dbcAreaGroup.LookupEntry( GetSpellProto()->AreaGroupId );

                for( uint8 i = 0; i < 7; i++ )
                {
                    if( GroupEntry->AreaId[i] != 0 && GroupEntry->AreaId[i] != area_id )
                    {
                        Extra = GroupEntry->AreaId[i];
                        break;
                    }
                }
            }
        }break;

    case SPELL_FAILED_TOTEMS:
        Extra = GetSpellProto()->Totem[1] ? GetSpellProto()->Totem[1] : GetSpellProto()->Totem[0];
        break;
    }

    if( Extra )
    {
        packetSMSG_CASTRESULT_EXTRA pe;
        pe.SpellId = GetSpellProto()->Id;
        pe.ErrorMessage = result;
        pe.MultiCast = extra_cast_number;
        pe.Extra = Extra;
        plr->GetSession()->OutPacket( SMSG_CAST_FAILED, sizeof( packetSMSG_CASTRESULT_EXTRA ), &pe );
    }
    else
    {
        packetSMSG_CASTRESULT pe;
        pe.SpellId = GetSpellProto()->Id;
        pe.ErrorMessage = result;
        pe.MultiCast = extra_cast_number;
        plr->GetSession()->OutPacket( SMSG_CAST_FAILED, sizeof( packetSMSG_CASTRESULT ), &pe );
    }
}

bool Spell::IsNeedSendToClient()
{
    if(u_caster == NULL || !m_caster->IsInWorld() || m_spellInfo->isPassiveSpell())
        return false;
    if(m_spellInfo->SpellVisual[0] || m_spellInfo->SpellVisual[1])
        return true;
    if(m_spellInfo->isChanneledSpell() || m_spellInfo->isChanneledSpell2())
        return true;
    if(m_spellInfo->speed > 0.0f)
        return true;
    if(!m_triggeredSpell)
        return true;
    return false;
}

void Spell::SendSpellStart()
{
    if(!IsNeedSendToClient())
        return;

    uint32 cast_flags = SPELL_CAST_FLAGS_CAST_DEFAULT;
    if(GetSpellProto()->powerType > 0 && GetSpellProto()->powerType != POWER_TYPE_HEALTH)
        cast_flags |= SPELL_CAST_FLAGS_POWER_UPDATE;
    if (m_spellInfo->RuneCostID && m_spellInfo->powerType == POWER_TYPE_RUNE)
        cast_flags |= SPELL_CAST_FLAGS_RUNIC_UPDATE;

    WorldPacket data(SMSG_SPELL_START, 150);
    if( i_caster != NULL )
        data << i_caster->GetGUID();
    else
        data << u_caster->GetGUID();

    data << u_caster->GetGUID();
    data << uint8(extra_cast_number);
    data << uint32(GetSpellProto()->Id);
    data << uint32(cast_flags);
    data << int32(m_timer);

    m_targets.write( data );

    if (cast_flags & SPELL_CAST_FLAGS_POWER_UPDATE)
        data << uint32(u_caster->GetPower(GetSpellProto()->powerType));
    m_caster->SendMessageToSet( &data, true );
}

void Spell::SendSpellGo()
{
    if(!IsNeedSendToClient())
        return;

    ItemPrototype* ip = NULL;
    uint32 cast_flags = SPELL_CAST_FLAGS_CAST_DEFAULT;
    if((m_triggeredSpell || m_triggeredByAura) && !m_spellInfo->isAutoRepeatSpell())
        cast_flags |= SPELL_CAST_FLAGS_NO_VISUAL;
    if(GetSpellProto()->powerType > 0 && GetSpellProto()->powerType != POWER_TYPE_HEALTH)
        cast_flags |= SPELL_CAST_FLAGS_POWER_UPDATE;
    if(m_missTargetCount)
        cast_flags |= SPELL_CAST_FLAGS_EXTRA_MESSAGE;
    if(p_caster && p_caster->getClass() == DEATHKNIGHT)
    {
        if(GetSpellProto()->RuneCostID)
        {
            cast_flags |= SPELL_CAST_FLAGS_RUNE_UPDATE;
            cast_flags |= 0x00040000;
        }
        else
        {
            for(uint8 i = 0; i < 3; i++)
            {
                if( GetSpellProto()->Effect[i] == SPELL_EFFECT_ACTIVATE_RUNE)
                {
                    cast_flags |= SPELL_CAST_FLAGS_RUNE_UPDATE;
                    cast_flags |= 0x00040000;
                }
            }
        }
    }

    WorldPacket data(SMSG_SPELL_GO, 200);
    if( i_caster != NULL ) // this is needed for correct cooldown on items
        data << i_caster->GetGUID();
    else
        data << m_caster->GetGUID();

    data << m_caster->GetGUID();
    data << extra_cast_number;
    data << GetSpellProto()->Id;
    data << cast_flags;
    data << getMSTime();

    writeSpellGoTargets(&data);

    m_targets.write( data ); // this write is included the target flag

    if (cast_flags & SPELL_CAST_FLAGS_POWER_UPDATE) //send new power
        data << uint32(u_caster->GetPower(GetSpellProto()->powerType));
    if( cast_flags & SPELL_CAST_FLAGS_RUNE_UPDATE ) //send new runes
    {
        SpellRuneCostEntry * runecost = dbcSpellRuneCost.LookupEntry(GetSpellProto()->RuneCostID);
        uint8 runeMask = p_caster->GetRuneMask(), theoretical = p_caster->TheoreticalUseRunes(runecost->bloodRuneCost, runecost->frostRuneCost, runecost->unholyRuneCost);
        data << runeMask << theoretical;
        for (uint8 i = 0; i < 6; i++)
        {
            uint8 mask = (1 << i);
            if (mask & runeMask && !(mask & theoretical))
                data << uint8(0);
        }
    }

    if (cast_flags & SPELL_CAST_FLAGS_PROJECTILE)
        data << m_missilePitch << m_missileTravelTime;

    if( m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION )
        data << uint8( 0 );

    m_caster->SendMessageToSet( &data, u_caster->IsPlayer() );
}

void Spell::SendProjectileUpdate()
{
    WorldPacket data(SMSG_SET_PROJECTILE_POSITION, 40);
    data << m_caster->GetGUID();
    data << extra_cast_number;
    data << float(0.0f) << float(0.0f) << float(0.0f);
    m_caster->SendMessageToSet(&data, true);
}

void Spell::writeSpellGoTargets( WorldPacket * data )
{
    SpellTargetMap::iterator itr;
    uint32 counter;

    // Make sure we don't hit over 100 targets.
    // It's fine internally, but sending it to the client will REALLY cause it to freak.

    *data << uint8(m_hitTargetCount);
    if( m_hitTargetCount > 0 )
    {
        counter = 0;
        for( itr = TargetMap.begin(); itr != TargetMap.end() && counter < 100; itr++ )
        {
            if( itr->second.HitResult == SPELL_DID_HIT_SUCCESS )
            {
                *data << itr->first;
                ++counter;
            }
        }
    }

    *data << uint8(m_missTargetCount);
    if( m_missTargetCount > 0 )
    {
        counter = 0;
        for( itr = TargetMap.begin(); itr != TargetMap.end() && counter < 100; itr++ )
        {
            if( itr->second.HitResult != SPELL_DID_HIT_SUCCESS )
            {
                *data << itr->first;
                *data << uint8(itr->second.HitResult);
                if (itr->second.HitResult == SPELL_DID_HIT_REFLECT)
                    *data << uint8(itr->second.ReflectResult);
                ++counter;
            }
        }
    }
}

void Spell::SendInterrupted(uint8 result)
{
    SetSpellFailed();

    if(!m_caster->IsInWorld()) 
        return;

    WorldPacket data(SMSG_SPELL_FAILURE, 13);
    data << m_caster->GetGUID();
    data << uint8(extra_cast_number);
    data << uint32(GetSpellProto()->Id);
    data << uint8(result);
    m_caster->SendMessageToSet(&data, true);

    data.Initialize(SMSG_SPELL_FAILED_OTHER);
    data << m_caster->GetGUID();
    data << uint8(extra_cast_number);
    data << uint32(GetSpellProto()->Id);
    data << uint8(result);
    m_caster->SendMessageToSet(&data, false);
}

void Spell::SendChannelUpdate(uint32 time)
{
    if(u_caster && time == 0)
    {
        if( u_caster->IsInWorld() && u_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT))
        {
            DynamicObject* dynObj = u_caster->GetMapMgr()->GetDynamicObject(u_caster->GetUInt32Value(UNIT_FIELD_CHANNEL_OBJECT));
            if(dynObj)
            {
                dynObj->RemoveFromWorld(true);
                dynObj->Destruct();
                dynObj = NULL;
            }
        }

        if( p_caster && p_caster->IsInWorld() && p_caster->GetUInt64Value(PLAYER_FARSIGHT) )
        {
            DynamicObject* dynObj = p_caster->GetMapMgr()->GetDynamicObject(p_caster->GetUInt32Value(PLAYER_FARSIGHT));
            if( dynObj )
            {
                dynObj->RemoveFromWorld(true);
                dynObj->Destruct();
                dynObj = NULL;
                p_caster->SetUInt32Value(PLAYER_FARSIGHT, 0);
            }
            p_caster->SetUInt64Value(PLAYER_FARSIGHT, 0);
            p_caster->GetMapMgr()->ChangeFarsightLocation(p_caster, p_caster->GetPositionX(), p_caster->GetPositionY(), false);
        }

        u_caster->SetChannelSpellTargetGUID(0);
        u_caster->SetChannelSpellId(0);
    }

    if(!p_caster)
        return;

    WorldPacket data(MSG_CHANNEL_UPDATE, 12);
    data << m_caster->GetGUID();
    data << time;
    p_caster->SendMessageToSet(&data, true);
}

void Spell::SendChannelStart(int32 duration)
{
    if (m_caster->GetTypeId() != TYPEID_GAMEOBJECT)
    {
        WorldPacket data(MSG_CHANNEL_START, 16);
        data << m_caster->GetGUID();
        data << GetSpellProto()->Id;
        data << duration;
        m_caster->SendMessageToSet(&data, true);
    }

    m_castTime = m_timer = duration;

    if( u_caster != NULL )
        u_caster->SetChannelSpellId(GetSpellProto()->Id);
}

void Spell::SendResurrectRequest(Player* target)
{
    const char* name = m_caster->IsCreature() ? castPtr<Creature>(m_caster)->GetCreatureData()->Name : "";
    WorldPacket data(SMSG_RESURRECT_REQUEST, 12+strlen(name)+3);
    data << m_caster->GetGUID();
    data << uint32(strlen(name) + 1);
    data << name;
    data << uint8(0);
    data << uint8(m_caster->IsCreature() ? 1 : 0);
    if (m_spellInfo->isResurrectionTimerIgnorant())
        data << uint32(0);
    target->GetSession()->SendPacket(&data);
}

bool Spell::HasPower()
{
    int32 powerField = 0;
    int32 cost = CalculateCost(powerField);
    if(powerField == -1)
        return false;
    if (cost <= 0)
    {
        m_usesMana = false; // no mana regen interruption for free spells
        return true;
    }
    // Unit has enough power (needed for creatures)
    return (cost <= m_caster->GetUInt32Value(powerField));
}

bool Spell::TakePower()
{
    int32 powerField = 0;
    int32 cost = CalculateCost(powerField);
    if(powerField == -1)
        return false;
    if (cost <= 0)
    {
        m_usesMana = false; // no mana regen interruption for free spells
        return true;
    }

    if(GetSpellProto()->powerType == POWER_TYPE_RUNIC)
    {
        SpellRuneCostEntry *runecost = dbcSpellRuneCost.LookupEntry(cost);
        p_caster->UseRunes(runecost->bloodRuneCost, runecost->frostRuneCost, runecost->unholyRuneCost, m_spellInfo);
        if(runecost->runePowerGain)
            u_caster->SetPower(POWER_TYPE_RUNIC, runecost->runePowerGain + u_caster->GetPower(POWER_TYPE_RUNIC));
    }
    else
    {
        int32 currentPower = m_caster->GetUInt32Value(powerField);
        if(powerField == UNIT_FIELD_HEALTH)
        {
            if(cost <= currentPower) // Unit has enough power (needed for creatures)
            {
                m_caster->DealDamage(u_caster, cost, 0, 0, 0,true);
                return true;
            }
        }
        else
        {
            if(cost <= currentPower) // Unit has enough power (needed for creatures)
            {
                if( u_caster && GetSpellProto()->powerType == POWER_TYPE_MANA )
                {
                    u_caster->m_LastSpellManaCost = cost;
                    if(m_spellInfo->IsSpellChannelSpell()) // Client only accepts channels
                        u_caster->DelayPowerRegeneration(GetDuration());
                }

                u_caster->SetPower(GetSpellProto()->powerType, currentPower - cost);
                return true;
            }
        }
    }
    return false;
}

int32 Spell::CalculateCost(int32 &powerField)
{
    // Initialize powerfield
    powerField = -1;
    // Only units use power, items do not
    if(u_caster == NULL || i_caster)
        return (powerField = 0);
    // Trainers can always cast, same with players with powercheat
    if(u_caster->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER) || (p_caster && p_caster->PowerCheat))
        return (powerField = 0);
    if((powerField = u_caster->GetPowerFieldForType(GetSpellProto()->powerType)) == -1)
        return 0;
    else if(GetSpellProto()->powerType == POWER_TYPE_MANA)
        m_usesMana = true;

    int32 cost = m_caster->GetSpellBaseCost(m_spellInfo);
    int32 currentPower = m_caster->GetUInt32Value(powerField);
    if( u_caster != NULL )
    {
        if( GetSpellProto()->isExhaustingPowerSpell() ) // Uses %100 mana
            return m_caster->GetUInt32Value(powerField);

        cost += u_caster->PowerCostMod[ m_usesMana ? GetSpellProto()->School : 0 ];//this is not percent!
        cost += float2int32(float(cost)* u_caster->GetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + GetSpellProto()->School));
    }

    //apply modifiers
    if( GetSpellProto()->SpellGroupType && u_caster)
    {
        u_caster->SM_FIValue(SMT_COST,&cost,GetSpellProto()->SpellGroupType);
        u_caster->SM_PIValue(SMT_COST,&cost,GetSpellProto()->SpellGroupType);
    }
    return cost;
}

WorldObject* Spell::_LookupObject(const WoWGuid& guid)
{
    if( guid == m_caster->GetGUID() )
        return m_caster;

    switch(GUID_HIPART(guid))
    {
    case HIGHGUID_TYPE_CORPSE:
        {
            return objmgr.GetCorpse(GUID_LOPART(guid));
        }break;
    default:
        {
            if( m_caster->IsInWorld() )
                return m_caster->GetMapMgr()->GetUnit(guid);
        }break;
    }

    return NULL;
}

void Spell::_SetTargets(WoWGuid guid)
{
    unitTarget = NULL;
    playerTarget = NULL;
    corpseTarget = NULL;
    gameObjTarget = NULL;
    itemTarget = NULL;

    if(!guid)
    {
        if(!m_caster->IsInWorld())
            return;

        if(p_caster != NULL)
        {
            if(m_targets.m_targetMask & TARGET_FLAG_ITEM)
                itemTarget = p_caster->GetItemInterface()->GetItemByGUID(m_targets.m_itemTarget);
            if(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
            {
                Player* p_trader = p_caster->GetTradeTarget();
                if(p_trader != NULL)
                    itemTarget = p_trader->getTradeItem(GUID_LOPART(m_targets.m_itemTarget));
            }
        }

        if(m_targets.m_targetMask & TARGET_FLAG_UNIT && m_targets.m_unitTarget)
        {
            MapMgr* mgr = m_caster->GetMapMgr();
            switch(GUID_HIPART(m_targets.m_unitTarget))
            {
            case HIGHGUID_TYPE_VEHICLE:
                unitTarget = mgr->GetVehicle(GUID_LOPART(m_targets.m_unitTarget));
                break;
            case HIGHGUID_TYPE_CREATURE:
                unitTarget = mgr->GetCreature(GUID_LOPART(m_targets.m_unitTarget));
                break;
            case HIGHGUID_TYPE_PET:
                unitTarget = mgr->GetPet(GUID_LOPART(m_targets.m_unitTarget));
                break;
            case HIGHGUID_TYPE_PLAYER:
                {
                    unitTarget = mgr->GetPlayer(GUID_LOPART(m_targets.m_unitTarget));
                    playerTarget = castPtr<Player>(unitTarget);
                }break;
            case HIGHGUID_TYPE_GAMEOBJECT:
                gameObjTarget = mgr->GetGameObject(GUID_LOPART(m_targets.m_unitTarget));
                break;
            case HIGHGUID_TYPE_CORPSE:
                corpseTarget = objmgr.GetCorpse(GUID_LOPART(m_targets.m_unitTarget));
                break;
            }
        }
    }
    else if(guid == m_caster->GetGUID())
    {
        unitTarget = u_caster;
        gameObjTarget = g_caster;
        playerTarget = p_caster;

        if(p_caster != NULL && m_targets.m_itemTarget)
        {
            if(m_targets.m_targetMask & TARGET_FLAG_ITEM)
                itemTarget = p_caster->GetItemInterface()->GetItemByGUID(m_targets.m_itemTarget);
            if(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
            {
                Player* p_trader = p_caster->GetTradeTarget();
                if(p_trader != NULL)
                    itemTarget = p_trader->getTradeItem(GUID_LOPART(m_targets.m_itemTarget));
            }
        }
    }
    else
    {
        if(!m_caster->IsInWorld())
            return;

        if(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
        {
            if(p_caster != NULL)
            {
                itemTarget = p_caster->GetItemInterface()->GetItemByGUID(guid);
                Player* plr = p_caster->GetTradeTarget();
                if(plr)
                    itemTarget = plr->getTradeItem(GUID_LOPART(guid));
            }
        }
        else
        {
            MapMgr* mgr = m_caster->GetMapMgr();
            switch(GUID_HIPART(guid))
            {
            case HIGHGUID_TYPE_VEHICLE:
                unitTarget = mgr->GetVehicle(GUID_LOPART(guid));
                break;
            case HIGHGUID_TYPE_CREATURE:
                unitTarget = mgr->GetCreature(GUID_LOPART(guid));
                break;
            case HIGHGUID_TYPE_PET:
                unitTarget = mgr->GetPet(GUID_LOPART(guid));
                break;
            case HIGHGUID_TYPE_PLAYER:
                {
                    unitTarget = mgr->GetPlayer(GUID_LOPART(guid));
                    playerTarget = castPtr<Player>(unitTarget);
                }break;
            case HIGHGUID_TYPE_GAMEOBJECT:
                gameObjTarget = mgr->GetGameObject(GUID_LOPART(guid));
                break;
            case HIGHGUID_TYPE_CORPSE:
                corpseTarget = objmgr.GetCorpse(GUID_LOPART(guid));
                break;
            case HIGHGUID_TYPE_ITEM:
                if( p_caster != NULL )
                    itemTarget = p_caster->GetItemInterface()->GetItemByGUID(guid);
                break;
            }
        }
    }
}

void Spell::HandleEffects(uint32 i)
{
    static_damage = false;
    bdamage = AdditionalCritChance = 0;
    damage += CalculateEffect(i, unitTarget, bdamage);
    sLog.Debug( "Spell","Handling Effect id = %u, damage = %d", GetSpellProto()->Effect[i], damage);

    uint32 effect = GetSpellProto()->Effect[i];
    sScriptMgr.HandleSpellEffectMod(GetSpellProto()->Id, i, this, effect);

    if( effect < TOTAL_SPELL_EFFECTS)
        (*this.*SpellEffectsHandler[effect])(i);
    else
        sLog.Debug("Spell","Unknown effect %u spellid %u", effect, GetSpellProto()->Id);
}

void Spell::HandleAddAura(WoWGuid guid)
{
    if(!guid)
        return;

    Unit* Target = NULL;
    if(u_caster && u_caster->GetGUID() == guid)
        Target = u_caster;
    else if(m_caster->IsInWorld())
        Target = m_caster->GetMapMgr()->GetUnit(guid);
    if(Target == NULL)
        return;

    // Applying an aura to a flagged target will cause you to get flagged.
    // self casting doesnt flag himself.
    if( p_caster && p_caster->GetGUID() != Target->GetGUID() )
    {
        if( Target->IsPvPFlagged() )
        {
            if( !p_caster->IsPvPFlagged() )
                p_caster->PvPToggle();
            else p_caster->SetPvPFlag();
        }
    }

    if( GetSpellProto()->MechanicsType == 31 )
        Target->SetFlag(UNIT_FIELD_AURASTATE, AURASTATE_FLAG_ENRAGE);

    // avoid map corruption
    if(Target->GetInstanceID() != m_caster->GetInstanceID())
        return;

    if(m_tempAuras.find(guid) != m_tempAuras.end())
    {
        Aura *aur = m_tempAuras.at(guid);

        // did our effects kill the target?
        if( Target->isDead() && !GetSpellProto()->isDeathPersistentAura())
        {
            // free pointer
            Target->RemoveAura(aur);
            m_tempAuras.erase(guid);
            return;
        }

        //make sure bg/arena preparation aura's are positive.
        if(GetSpellProto()->Id == 32727 || GetSpellProto()->Id == 44521)
            aur->SetPositive();
        Target->AddAura(aur);
        m_tempAuras.erase(guid);
    }
}

bool Spell::IsAspect()
{
    return GetSpellProto()->isSpellAspectBuff();
}

bool Spell::IsSeal()
{
    return GetSpellProto()->isSpellSealBuff();
}

bool Spell::IsBinary(SpellEntry * sp)
{
    // Normally, damage spells are only binary if they have an additional non-damage effect
    // DoTs used to be binary spells, but this was changed. (WoWwiki)
    return !(sp->Effect[0] == SPELL_EFFECT_SCHOOL_DAMAGE || sp->EffectApplyAuraName[0] == SPELL_AURA_PERIODIC_DAMAGE);
}

uint8 Spell::CanCast(bool tolerate)
{
    bool skip = (p_caster && (p_caster->m_skipCastCheck[0] & GetSpellProto()->SpellGroupType[0] ||
        p_caster && p_caster->m_skipCastCheck[1] & GetSpellProto()->SpellGroupType[1] ||
        p_caster && p_caster->m_skipCastCheck[2] & GetSpellProto()->SpellGroupType[2])); // related to aura 262

    if(objmgr.IsSpellDisabled(GetSpellProto()->Id))
        return SPELL_FAILED_SPELL_UNAVAILABLE;

    if( u_caster && u_caster->GetCurrentSpell() != NULL && u_caster->GetCurrentSpell() != this )
        return SPELL_FAILED_SPELL_IN_PROGRESS;

    /* Spells for the zombie event */
    if( p_caster && p_caster->GetShapeShift() ==FORM_ZOMBIE && !( ((uint32)1 << (p_caster->GetShapeShift()-1)) & GetSpellProto()->RequiredShapeShift  ))
    {
        sLog.outDebug("Invalid shapeshift: %u", GetSpellProto()->RequiredShapeShift);
        return SPELL_FAILED_SPELL_UNAVAILABLE;
    }

    if((m_castTime || m_spellInfo->IsSpellChannelSpell()) && p_caster && p_caster->m_isMoving)
        if((GetSpellProto()->InterruptFlags & CAST_INTERRUPT_ON_MOVEMENT) || m_spellInfo->ChannelInterruptFlags & CHANNEL_INTERRUPT_ON_MOVEMENT)
            return SPELL_FAILED_MOVING;

    if(m_caster->IsInWorld())
    {
        Unit *target = m_caster->GetMapMgr()->GetUnit( m_targets.m_unitTarget );
        if( target )
        {
            if(target->IsCreature())
            {
                Creature* cTarget = castPtr<Creature>(target);
                if(isTargetDummy(cTarget->GetEntry()))
                {
                    switch(m_spellInfo->Id)
                    {
                    case 49576:
                        {
                            return SPELL_FAILED_BAD_TARGETS;
                        }break;
                    }
                }
            }

            if( GetSpellProto()->Id == 48788 && target->GetHealthPct() == 100)
                return SPELL_FAILED_ALREADY_AT_FULL_HEALTH;

            // GM flagged players should be immune to other players' casts, but not their own.
            if(target->IsPlayer() && (m_caster->GetTypeId() == TYPEID_ITEM ? (castPtr<Item>(m_caster)->GetOwner() != target) : (m_caster != target)) && castPtr<Player>(target)->bGMTagOn)
                return SPELL_FAILED_BM_OR_INVISGOD;

            //you can't mind control someone already mind controlled
            if (GetSpellProto()->NameHash == SPELL_HASH_MIND_CONTROL && target->m_AuraInterface.GetAuraSpellIDWithNameHash(SPELL_HASH_MIND_CONTROL))
                return SPELL_FAILED_CANT_BE_CHARMED;

            //these spells can be cast only on certain objects. Otherwise cool exploit
            //Most of this things goes to spell_forced_target table
            switch (GetSpellProto()->Id)
            {
                case 27907:// Disciplinary Rod
                {
                    if( target->IsPlayer() )
                        return SPELL_FAILED_BAD_TARGETS;
                }break;
            }
        }
    }

    if( p_caster != NULL )
    {
        if( GetSpellProto()->Id == 51721 )
        {
            if(p_caster->GetAreaId() != 4281)
                return SPELL_FAILED_NOT_HERE;
        }

        if( GetSpellProto()->NameHash == SPELL_HASH_LIFE_TAP )
        {
            if(p_caster->GetPowerPct(POWER_TYPE_MANA) == 100)
                return SPELL_FAILED_ALREADY_AT_FULL_POWER;
        }

        // flying auras
        if( GetSpellProto()->isSpellFlyingSpell() )
        {
            if(!p_caster->CanFlyInCurrentZoneOrMap()) // Check our area
                return SPELL_FAILED_NOT_HERE;
        }
        else
        {
            if(GetSpellProto()->isSpellCastableOnlyInOutlands() && p_caster->GetMapId() != 530)
                return SPELL_FAILED_NOT_HERE;
        }

        if( GetSpellProto()->Id == 53822 && p_caster->getClass()!=DEATHKNIGHT)          // DeathGate
            return SPELL_FAILED_SPELL_UNAVAILABLE;

        if(p_caster->HasFlag(PLAYER_FLAGS,PLAYER_FLAG_ALLOW_ONLY_ABILITY) &&
            !((GetSpellProto()->SpellGroupType[0] & p_caster->m_castFilter[0]) ||
            (GetSpellProto()->SpellGroupType[1] & p_caster->m_castFilter[1]) ||
            (GetSpellProto()->SpellGroupType[2] & p_caster->m_castFilter[2])))
            return SPELL_FAILED_SPELL_IN_PROGRESS;  // Need to figure the correct message

        uint32 self_rez = p_caster->GetUInt32Value(PLAYER_SELF_RES_SPELL);
        // if theres any spells that should be cast while dead let me know
        if( !p_caster->isAlive() && self_rez != GetSpellProto()->Id)
        {
            if( (m_targets.m_targetMask & TARGET_FLAG_SELF  || m_targets.m_unitTarget == p_caster->GetGUID() || !IsHealingSpell(m_spellInfo)) && p_caster->GetShapeShift() == FORM_SPIRITOFREDEMPTION)      // not a holy spell
                    return SPELL_FAILED_SPELL_UNAVAILABLE;

            if(!GetSpellProto()->isCastableWhileDead())
                return SPELL_FAILED_NOT_WHILE_GHOST;
        }

        if( GetSpellProto()->NameHash == SPELL_HASH_HUNTER_S_MARK )
        {
            if( GetUnitTarget() && !sFactionSystem.isHostile( GetUnitTarget(), m_caster ))
                return SPELL_FAILED_BAD_TARGETS;
        }

        if (p_caster->GetMapMgr() && p_caster->GetMapMgr()->CanUseCollision(p_caster))
        {
            if (GetSpellProto()->MechanicsType == MECHANIC_MOUNTED)
            {
                // Qiraj battletanks work everywhere on map 531
                if ( p_caster->GetMapId() == 531 && ( GetSpellProto()->Id == 25953 || GetSpellProto()->Id == 26054 || GetSpellProto()->Id == 26055 || GetSpellProto()->Id == 26056 ) )
                    return SPELL_CANCAST_OK;

                if (sVMapInterface.IsIndoor( p_caster->GetMapId(), p_caster->GetPositionX(), p_caster->GetPositionY(), p_caster->GetPositionZ() + 2.0f ))
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;
            }
            else if( GetSpellProto()->isOutdoorOnly() )
            {
                if(sVMapInterface.IsIndoor( p_caster->GetMapId(),p_caster->GetPositionX(), p_caster->GetPositionY(), p_caster->GetPositionZ() + 2.0f ) )
                    return SPELL_FAILED_ONLY_OUTDOORS;
            }
        }
        //are we in an arena and the spell cooldown is longer then 15mins?
        if ( p_caster->m_bg && ( p_caster->m_bg->GetType() >= BATTLEGROUND_ARENA_2V2 && p_caster->m_bg->GetType() <= BATTLEGROUND_ARENA_5V5 ) )
        {
            if( GetSpellProto()->isSpellNotAvailableInArena() )
                return SPELL_FAILED_NOT_IN_ARENA;

            if( !p_caster->m_bg->HasStarted() )
            {
                // cannot cast spells in an arena if it hasn't started (blinking through gates, lalala)
                if( GetSpellProto()->NameHash == SPELL_HASH_BLINK )
                    return SPELL_FAILED_SPELL_UNAVAILABLE;
            }
        }

        // What a waste.
        if( p_caster->m_bg && GetGameObjectTarget())
        {
            if(p_caster->SchoolImmunityList[0]) // Physical is all that really matters.
                return SPELL_FAILED_SPELL_UNAVAILABLE;
        }

        if(p_caster->m_bg == NULL && GetSpellProto()->reqInBattleground())
            return SPELL_FAILED_ONLY_BATTLEGROUNDS;

        // Requires ShapeShift (stealth only atm, need more work)
        if( GetSpellProto()->RequiredShapeShift )
        {
            if( GetSpellProto()->RequiredShapeShift == (uint32)1 << (FORM_STEALTH-1) )
            {
                if( !(((uint32)1 << (p_caster->GetShapeShift()-1)) & GetSpellProto()->RequiredShapeShift) && !p_caster->HasDummyAura(SPELL_HASH_SHADOW_DANCE) )
                    return SPELL_FAILED_ONLY_STEALTHED;
            }
        }

        if(!u_caster->CombatStatus.IsInCombat() && GetSpellProto()->NameHash == SPELL_HASH_DISENGAGE)
            return SPELL_FAILED_SPELL_UNAVAILABLE;;

        // Disarm
        if( u_caster!= NULL )
        {
            if (GetSpellProto()->isUnavailableInCombat() && u_caster->CombatStatus.IsInCombat())
            {
                // Charge In Combat, it's broke since 3.3.5, ??? maybe an aura state that needs to be set now
                //if ((GetSpellProto()->Id !=  100 && GetSpellProto()->Id != 6178 && GetSpellProto()->Id != 11578 ) )
                return SPELL_FAILED_TARGET_IN_COMBAT;
            }


            if( GetSpellProto()->IsSpellWeaponSpell() && u_caster->disarmed )
                return SPELL_FAILED_EQUIPPED_ITEM_CLASS;
            if( u_caster->disarmedShield && GetSpellProto()->RequiredItemFlags && (GetSpellProto()->RequiredItemFlags & (1 << INVTYPE_SHIELD)) )
                return SPELL_FAILED_EQUIPPED_ITEM_CLASS;
        }

        // check for cooldowns
        if(!tolerate && !p_caster->Cooldown_CanCast(m_spellInfo))
            return SPELL_FAILED_NOT_READY;

        if(p_caster->GetDuelState() == DUEL_STATE_REQUESTED)
        {
            for(uint8 i = 0; i < 3; i++)
            {
                if( GetSpellProto()->Effect[i] && GetSpellProto()->Effect[i] != SPELL_EFFECT_APPLY_AURA
                    && GetSpellProto()->Effect[i] != SPELL_EFFECT_APPLY_PET_AURA
                    && GetSpellProto()->Effect[i] != SPELL_EFFECT_APPLY_AREA_AURA)
                {
                    return SPELL_FAILED_TARGET_DUELING;
                }
            }
        }

        // check for duel areas
        if( GetSpellProto()->Id == 7266 )
        {
            if(p_caster->GetMapMgr()->CanUseCollision(p_caster))
            {
                if(sVMapInterface.IsIncity(p_caster->GetMapId(), p_caster->GetPositionX(), p_caster->GetPositionY(), p_caster->GetPositionZ()))
                    return SPELL_FAILED_NO_DUELING;
            }
            else
            {
                AreaTableEntry* at = dbcAreaTable.LookupEntry( p_caster->GetAreaId() );
                if(at == NULL)
                    at = dbcAreaTable.LookupEntry( p_caster->GetZoneId() );

                if(at != NULL && at->AreaFlags & AREA_CITY_AREA)
                    return SPELL_FAILED_NO_DUELING;
            }

            if( p_caster->m_bg )
                return SPELL_FAILED_NO_DUELING;
        }

        // check if spell is allowed while player is on a taxi
        if(p_caster->m_onTaxi)
        {
            // This uses the same flag as ordinary mounts
            if(!GetSpellProto()->isCastableWhileMounted())
                return SPELL_FAILED_NOT_ON_TAXI;
        }

        // check if spell is allowed while not mounted
        if(!p_caster->IsMounted())
        {
            if( GetSpellProto()->Id == 25860) // Reindeer Transformation
                return SPELL_FAILED_ONLY_MOUNTED;
        }
        else
        {
            if (!GetSpellProto()->isCastableWhileMounted())
                return SPELL_FAILED_NOT_MOUNTED;
        }

        // no mana drains on shifted druids :(
        if( GetPlayerTarget() && GetPlayerTarget()->getClass() == DRUID && (GetSpellProto()->Effect[0] == SPELL_EFFECT_POWER_DRAIN || GetSpellProto()->Effect[1] == SPELL_EFFECT_POWER_DRAIN || GetSpellProto()->Effect[2] == SPELL_EFFECT_POWER_DRAIN))
        {
            if( GetPlayerTarget()->GetShapeShift() == FORM_BEAR ||
                GetPlayerTarget()->GetShapeShift() == FORM_DIREBEAR ||
                GetPlayerTarget()->GetShapeShift() == FORM_CAT)
                return SPELL_FAILED_BAD_TARGETS;
        }

        // check if spell is allowed while shapeshifted
        if(p_caster->GetShapeShift())
        {
            switch(p_caster->GetShapeShift())
            {
            case FORM_TREE:
            case FORM_BATTLESTANCE:
            case FORM_DEFENSIVESTANCE:
            case FORM_BERSERKERSTANCE:
            case FORM_SHADOW:
            case FORM_STEALTH:
            case FORM_MOONKIN:
                break;

            case FORM_SWIFT:
            case FORM_FLIGHT:
                {
                    // check if item is allowed (only special items allowed in flight forms)
                    if(i_caster && !(i_caster->GetProto()->Flags & ITEM_FLAG_SHAPESHIFT_OK))
                        return SPELL_FAILED_NO_ITEMS_WHILE_SHAPESHIFTED;
                }break;

            default:
                {
                    // check if item is allowed (only special & equipped items allowed in other forms)
                    if(i_caster && !(i_caster->GetProto()->Flags & ITEM_FLAG_SHAPESHIFT_OK))
                        if(i_caster->GetProto()->InventoryType == INVTYPE_NON_EQUIP)
                            return SPELL_FAILED_NO_ITEMS_WHILE_SHAPESHIFTED;
                }
            }
        }


        for(uint8 i = 0; i < 3; i++)
        {
            if( GetSpellProto()->Effect[i] == SPELL_EFFECT_OPEN_LOCK && GetSpellProto()->EffectMiscValue[i] == LOCKTYPE_SLOW_OPEN )
            {
                if( p_caster->m_MountSpellId )
                    p_caster->RemoveAura( p_caster->m_MountSpellId );

                if( p_caster->GetVehicle() )
                    p_caster->GetVehicle()->RemovePassenger( p_caster );

                if( p_caster->m_stealth )
                {
                    p_caster->m_AuraInterface.RemovePositiveAura( p_caster->m_stealth );
                    p_caster->m_AuraInterface.RemoveAuraByNameHash( SPELL_HASH_VANISH );
                }
                break;
            }
        }

        // check if spell is allowed while we have a battleground flag
        if(p_caster->m_bgHasFlag)
        {
            if( (GetSpellProto()->NameHash == SPELL_HASH_DIVINE_SHIELD || GetSpellProto()->NameHash == SPELL_HASH_ICE_BLOCK) )
            {
                if(p_caster->m_bg && p_caster->m_bg->GetType() == BATTLEGROUND_WARSONG_GULCH)
                    castPtr<WarsongGulch>(p_caster->m_bg)->DropFlag( p_caster );
                else if(p_caster->m_bg && p_caster->m_bg->GetType() == BATTLEGROUND_EYE_OF_THE_STORM)
                    castPtr<EyeOfTheStorm>(p_caster->m_bg)->DropFlag( p_caster );
            }
        }

        // item spell checks
        if(i_caster != NULL)
        {
            if( i_caster->GetProto()->MapID && i_caster->GetProto()->MapID != i_caster->GetOwner()->GetMapId() )
                return SPELL_FAILED_NOT_HERE;

            if(i_caster->GetProto()->Spells[0].Charges != 0)
            {
                // check if the item has the required charges
                if(i_caster->GetUInt32Value(ITEM_FIELD_SPELL_CHARGES) == 0)
                {
                    //Mounts have changed, they should be added to known spells
                    if(i_caster->GetProto()->Class != ITEM_CLASS_MISCELLANEOUS && i_caster->GetProto()->SubClass != ITEM_SUBCLASS_MISCELLANEOUS_MOUNT )
                        return SPELL_FAILED_SPELL_LEARNED;
                    else
                    {
                        Unit* target = (m_caster->IsInWorld()) ? m_caster->GetMapMgr()->GetUnit(m_targets.m_unitTarget) : NULL;
                        if(target && target->IsPlayer())
                        {
                            //Allow spell to be casted if player didn't have this mount yet in pet tab (iow has the spell).
                            if(i_caster->GetProto()->Spells[1].Id && castPtr<Player>(target)->HasSpell(i_caster->GetProto()->Spells[1].Id))
                                return SPELL_FAILED_SPELL_LEARNED;
                        }
                    }
                }

                // for items that combine to create a new item, check if we have the required quantity of the item
                if(i_caster->GetProto()->ItemId == GetSpellProto()->Reagent[0] && (i_caster->GetProto()->Flags != 268435520))
                    if(p_caster->GetItemInterface()->GetItemCount(GetSpellProto()->Reagent[0]) < GetSpellProto()->ReagentCount[0] + 1)
                        return SPELL_FAILED_NEED_MORE_ITEMS;
            }

            // heal checks are only applied to item casters
            if( GetSpellProto()->isSpellHealingEffect() && p_caster->GetUInt32Value(UNIT_FIELD_HEALTH) == p_caster->GetUInt32Value(UNIT_FIELD_MAXHEALTH) )
                return SPELL_FAILED_ALREADY_AT_FULL_HEALTH;

            if( p_caster->getPowerType() == POWER_TYPE_MANA && GetSpellProto()->isSpellEnergizingEffect() && p_caster->GetPowerPct(POWER_TYPE_MANA) == 100 )
                return SPELL_FAILED_ALREADY_AT_FULL_MANA;
        }

        bool CheckReagents = true;
        if( GetSpellProto()->SpellGroupType )
        {
            //lets check if we need regeants
            uint32 AffectedSpellGroupType[3] = {0,0,0};
            for(uint32 x=0;x<3;x++)
                AffectedSpellGroupType[x] |= p_caster->GetUInt32Value(PLAYER_NO_REAGENT_COST_1+x);

            if( AffectedSpellGroupType )
            {
                for(uint32 x=0;x<3;x++)
                    if( AffectedSpellGroupType[x] & GetSpellProto()->SpellGroupType[x] )
                        CheckReagents = false;
            }
        }

        // check if we have the required reagents
        if( CheckReagents && (!i_caster || (i_caster->GetProto() && i_caster->GetProto()->Flags != 268435520)) && !p_caster->NoReagentCost)
        {
            for(uint8 i=0; i<8 ;++i)
            {
                if( GetSpellProto()->Reagent[i] <= 0 || GetSpellProto()->ReagentCount[i] <= 0)
                    continue;

                if(p_caster->GetItemInterface()->GetItemCount(GetSpellProto()->Reagent[i]) < GetSpellProto()->ReagentCount[i])
                    return SPELL_FAILED_NEED_MORE_ITEMS;
            }
        }

        // check if we have the required tools, totems, etc
        if( GetSpellProto()->Totem[0] != 0)
        {
            if(!p_caster->GetItemInterface()->GetItemCount(GetSpellProto()->Totem[0]))
                return SPELL_FAILED_TOTEMS;
        }

        if( GetSpellProto()->Totem[1] != 0)
        {
            if(!p_caster->GetItemInterface()->GetItemCount(GetSpellProto()->Totem[1]))
                return SPELL_FAILED_TOTEMS;
        }

        // stealth check
        if( GetSpellProto()->NameHash == SPELL_HASH_STEALTH || GetSpellProto()->NameHash == SPELL_HASH_PROWL )
        {
            if( p_caster->CombatStatus.IsInCombat() )
                return SPELL_FAILED_TARGET_IN_COMBAT;
        }

        if( ( GetSpellProto()->NameHash == SPELL_HASH_CANNIBALIZE || GetSpellProto()->Id == 46584 ))
        {
            bool check = false;
            for(WorldObject::InRangeSet::iterator i = p_caster->GetInRangeSetBegin(); i != p_caster->GetInRangeSetEnd(); i++)
            {
                if(p_caster->GetDistance2dSq((*i)) <= 25)
                    if((*i)->GetTypeId() == TYPEID_UNIT || (*i)->IsPlayer() )
                        if( castPtr<Unit>(*i)->isDead() )
                            check = true;
            }

            if( !check && GetSpellProto()->Id == 46584 )
            {
                if( p_caster->HasDummyAura( SPELL_HASH_GLYPH_OF_RAISE_DEAD ) )
                    check = true;
                else if( p_caster->GetItemInterface()->GetItemCount( 37201 ) )
                {
                    check = true;
                    p_caster->GetItemInterface()->RemoveItemAmt( 37201, 1 );
                }
            }
            if( !check )
            {
                return SPELL_FAILED_NO_EDIBLE_CORPSES;
            }
        }

        // check if we have the required gameobject focus
        if( GetSpellProto()->RequiresSpellFocus)
        {
            float focusRange;
            bool found = false;
            for( std::unordered_set<WorldObject*>::iterator itr = p_caster->GetInRangeSetBegin(); itr != p_caster->GetInRangeSetEnd(); itr++ )
            {
                if(found == true)
                    break;

                if((*itr)->GetTypeId() != TYPEID_GAMEOBJECT)
                    continue;

                if(castPtr<GameObject>(*itr)->GetType() != GAMEOBJECT_TYPE_SPELL_FOCUS)
                    continue;

                GameObjectInfo *info = castPtr<GameObject>(*itr)->GetInfo();
                if(!info)
                {
                    sLog.Debug("Spell","Warning: could not find info about game object %u", (*itr)->GetEntry());
                    continue;
                }

                // lets read the distance from database
                focusRange = (float)info->TypeSpellFocus.Dist;

                // is that possible?
                if( !focusRange )
                    focusRange = 5.0f;

                if(!IsInrange(p_caster->GetPositionX(), p_caster->GetPositionY(), p_caster->GetPositionZ(), (*itr), (focusRange * focusRange)))
                    continue;

                if(info->TypeSpellFocus.FocusId == GetSpellProto()->RequiresSpellFocus)
                {
                    found = true;
                    break;
                }
            }

            if(!found)
                return SPELL_FAILED_REQUIRES_SPELL_FOCUS;
        }

        if( GetSpellProto()->AreaGroupId > 0)
        {
            bool found = false;
            uint16 area_id = p_caster->GetAreaId();
            uint32 zone_id = p_caster->GetZoneId();

            AreaGroupEntry *groupEntry = dbcAreaGroup.LookupEntry( GetSpellProto()->AreaGroupId );
            if( groupEntry )
            {
                for ( uint8 i=0; i<7; i++ )
                {
                    if( groupEntry->AreaId[i] == zone_id || groupEntry->AreaId[i] == area_id )
                    {
                        found = true;
                        break;
                    }
                }
            }

            if(!found)
                return SPELL_FAILED_REQUIRES_AREA;
        }

        // aurastate check
        if( GetSpellProto()->CasterAuraState)
        {
            if( !p_caster->HasFlag( UNIT_FIELD_AURASTATE, 1 << (GetSpellProto()->CasterAuraState-1) ) )
                return SPELL_FAILED_CASTER_AURASTATE;
        }
    }

    // Targetted Item Checks
    if(m_targets.m_itemTarget && p_caster)
    {
        Item* i_target = NULL;

        // check if the targeted item is in the trade box
        if( m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM )
        {
                // only lockpicking and enchanting can target items in the trade box
            if(HasSpellEffect(SPELL_EFFECT_OPEN_LOCK) || HasSpellEffect(SPELL_EFFECT_ENCHANT_ITEM) || HasSpellEffect(SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY))
            {
                // check for enchants that can only be done on your own items
                if( GetSpellProto()->isTargetOwnItemsOnly() )
                    return SPELL_FAILED_BAD_TARGETS;

                // get the player we are trading with
                Player* t_player = p_caster->GetTradeTarget();
                // get the targeted trade item
                if( t_player != NULL )
                    i_target = t_player->getTradeItem(m_targets.m_targetIndex);
            }
        } // targeted item is not in a trade box, so get our own item
        else i_target = p_caster->GetItemInterface()->GetItemByGUID( m_targets.m_itemTarget );

        // check to make sure we have a targeted item
        if( i_target == NULL )
            return SPELL_FAILED_BAD_TARGETS;

        ItemPrototype* proto = i_target->GetProto();

        // check to make sure we have it's prototype info
        if(!proto) 
            return SPELL_FAILED_BAD_TARGETS;

        // check to make sure the targeted item is acceptable
        // Enchanting Targeted Item Check
        if(HasSpellEffect(SPELL_EFFECT_ENCHANT_ITEM) || HasSpellEffect(SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY))
        {
            // check for enchants that can only be done on your own items, make sure they are soulbound
            if( GetSpellProto()->isTargetOwnItemsOnly() && i_target->GetOwner() != p_caster)
                return SPELL_FAILED_BAD_TARGETS;
            // check if we have the correct class, subclass, and inventory type of target item
            if( GetSpellProto()->EquippedItemClass != (int32)proto->Class && proto->Class != 7)
                return SPELL_FAILED_BAD_TARGETS;

            if( GetSpellProto()->EquippedItemSubClass && !(GetSpellProto()->EquippedItemSubClass & (1 << proto->SubClass)) &&  GetSpellProto()->EffectMiscValueB[0] != (int32)proto->SubClass )
                return SPELL_FAILED_BAD_TARGETS;

            if( GetSpellProto()->RequiredItemFlags && !(GetSpellProto()->RequiredItemFlags & (1 << proto->InventoryType)) && proto->InventoryType != 0 )
                return SPELL_FAILED_BAD_TARGETS;

            if(proto->ItemLevel > 0)
            {
                if (GetSpellProto()->Effect[0] == SPELL_EFFECT_ENCHANT_ITEM &&
                    GetSpellProto()->baseLevel && (GetSpellProto()->baseLevel > (uint32)proto->ItemLevel))
                    return int8(SPELL_FAILED_BAD_TARGETS); // maybe there is different err code
            }
        }

            // Disenchanting Targeted Item Check
        if(HasSpellEffect(SPELL_EFFECT_DISENCHANT))
        {
            // check if item can be disenchanted
            if(proto->DisenchantReqSkill < 0)
                return SPELL_FAILED_CANT_BE_DISENCHANTED;
            // check if we have high enough skill
            if((int32)p_caster->_GetSkillLineCurrent(SKILL_ENCHANTING) < proto->DisenchantReqSkill)
                return SPELL_FAILED_CANT_BE_DISENCHANTED_SKILL;
        }

        // Feed Pet Targeted Item Check
        if(HasSpellEffect(SPELL_EFFECT_FEED_PET))
        {
            Pet* pPet = p_caster->GetSummon();
            // check if we have a pet
            if(!pPet)
                return SPELL_FAILED_NO_PET;
            // check if item is food
            if(!proto->FoodType)
                return SPELL_FAILED_BAD_TARGETS;

            // check if food type matches pets diet
            if(!(pPet->GetPetDiet() & (1 << (proto->FoodType - 1))))
                return SPELL_FAILED_WRONG_PET_FOOD;
            // check food level: food should be max 30 lvls below pets level
            if(proto->ItemLevel < 0 || pPet->getLevel() > (uint32)proto->ItemLevel + 30)
                return SPELL_FAILED_FOOD_LOWLEVEL;
        }

            // Prospecting Targeted Item Check
        if(HasSpellEffect(SPELL_EFFECT_PROSPECTING))
        {
            // check if the item can be prospected
            if(!(proto->Flags & ITEM_FLAG_PROSPECTABLE))
                return SPELL_FAILED_CANT_BE_PROSPECTED;
            // check if we have at least 5 of the item
            if(p_caster->GetItemInterface()->GetItemCount(proto->ItemId) < 5)
                return SPELL_FAILED_NEED_MORE_ITEMS;
            // check if we have high enough skill
            if(proto->RequiredSkillRank < 0 || p_caster->_GetSkillLineCurrent(SKILL_JEWELCRAFTING) < (uint32)proto->RequiredSkillRank)
                return SPELL_FAILED_LOW_CASTLEVEL;
        }

            // Milling Targeted Item Check
        if(HasSpellEffect(SPELL_EFFECT_MILLING))
        {
            // check if the item can be milled
            if(!(proto->Flags & ITEM_FLAG_MILLABLE))
                return SPELL_FAILED_CANT_BE_MILLED;
            // check if we have at least 5 of the item
            if(p_caster->GetItemInterface()->GetItemCount(proto->ItemId) < 5)
                return SPELL_FAILED_NEED_MORE_ITEMS;
            // check if we have high enough skill
            if(proto->RequiredSkillRank < 0 || p_caster->_GetSkillLineCurrent(SKILL_INSCRIPTION) < (uint32)proto->RequiredSkillRank)
                return SPELL_FAILED_LOW_CASTLEVEL;
        }
    }

    // set up our max Range
    float maxRange = GetSpellProto()->Range[2];
    if(m_targets.m_unitTarget && m_caster && m_caster->IsInWorld())
        if(sFactionSystem.isCombatSupport(u_caster, m_caster->GetMapMgr()->GetUnit(m_targets.m_unitTarget)))
            maxRange = GetSpellProto()->Range[3];

    if( GetSpellProto()->SpellGroupType && u_caster != NULL )
    {
        u_caster->SM_FFValue(SMT_RANGE, &maxRange, GetSpellProto()->SpellGroupType );
        u_caster->SM_PFValue(SMT_RANGE, &maxRange, GetSpellProto()->SpellGroupType );
    }

    // Targeted Location Checks (AoE spells)
    if( m_targets.m_targetMask == TARGET_FLAG_DEST_LOCATION )
    {
        if( !IsInrange( m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, m_caster, ( maxRange * maxRange ) ) )
            return SPELL_FAILED_OUT_OF_RANGE;
    }

    // Collision 2 broken for this :|
    //if(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION && !m_caster->IsInLineOfSight(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ))
        //return SPELL_FAILED_LINE_OF_SIGHT;

    Unit* target = NULL;
    if( m_targets.m_targetMask == TARGET_FLAG_SELF )
        target = u_caster;

    // Targeted Unit Checks
    if(m_targets.m_unitTarget)
    {
        if( m_targets.m_unitTarget == m_caster->GetGUID() && m_caster->IsUnit() )
            target = castPtr<Unit>(m_caster);
        else
            target = (m_caster->IsInWorld()) ? m_caster->GetMapMgr()->GetUnit(m_targets.m_unitTarget) : NULL;

        if(target != NULL)
        {
            if( g_caster != NULL )
            {
                if(target->SchoolImmunityList[GetSpellProto()->School])
                    return SPELL_FAILED_DAMAGE_IMMUNE;

                if(target->GetMechanicDispels(GetSpellProto()->MechanicsType))
                    return SPELL_FAILED_DAMAGE_IMMUNE;
            }

            if( target != m_caster )
            {
                // Partha: +2.52yds to max range, this matches the range the client is calculating.
                // see extra/supalosa_range_research.txt for more info

                if( tolerate ) // add an extra 33% to range on final check (squared = 1.78x)
                {
                    if( !IsInrange( m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ(), target, ( ( maxRange + 2.52f ) * ( maxRange + 2.52f ) * 1.78f ) ) )
                        return SPELL_FAILED_OUT_OF_RANGE;
                }
                else
                {
                    // Added +2 because there's always someone who forgot to put CombatReach into the DB and latency compensation
                    float targetRange = maxRange + target->GetSize() + (u_caster ? u_caster->GetSize() : 0 ) + 2;
                    if( !IsInrange(m_caster, target, targetRange * targetRange ) )
                        return SPELL_FAILED_OUT_OF_RANGE;
                }
            }

            if( p_caster != NULL )
            {
                /*if( GetSpellProto()->forced_creature_target )
                {
                    if( !target->IsCreature() )
                        return SPELL_FAILED_BAD_TARGETS;

                    if( castPtr<Creature>( target )->GetCreatureData() != NULL )
                        if( GetSpellProto()->forced_creature_target != castPtr<Creature>( target )->GetEntry() )
                            return SPELL_FAILED_BAD_TARGETS;
                }*/

                if( GetSpellProto()->Id == SPELL_RANGED_THROW)
                {
                    Item* itm = p_caster->GetItemInterface()->GetInventoryItem(EQUIPMENT_SLOT_RANGED);
                    if(!itm || ((itm->GetDurability() == 0) && itm->GetDurabilityMax()))
                        return SPELL_FAILED_NO_AMMO;
                }

                if ( target != m_caster && !m_caster->IsInLineOfSight(target) )
                {
                    return SPELL_FAILED_LINE_OF_SIGHT;
                }


                // check aurastate
                if( GetSpellProto()->TargetAuraState && !skip)
                {
                    if( !target->HasFlag( UNIT_FIELD_AURASTATE, 1<<(GetSpellProto()->TargetAuraState-1) ) )
                    {
                        return SPELL_FAILED_TARGET_AURASTATE;
                    }
                }

                if(target->IsPlayer())
                {
                    // disallow spell casting in sanctuary zones
                    // allow attacks in duels
                    if( p_caster->DuelingWith != target && !sFactionSystem.isFriendly( p_caster, target ) )
                    {
                        AreaTableEntry* atCaster = dbcAreaTable.LookupEntry( p_caster->GetAreaId() );
                        AreaTableEntry* atTarget = dbcAreaTable.LookupEntry( target->GetAreaId() );
                        if( atCaster != NULL && atTarget != NULL )
                            if( atCaster->AreaFlags & 0x800 || atTarget->AreaFlags & 0x800 )
                                return SPELL_FAILED_NOT_HERE;
                    }
                }

                if( GetSpellProto()->EffectApplyAuraName[0] == 2)//mind control
                {
                    if( GetSpellProto()->EffectBasePoints[0])//got level req;
                    {
                        if((int32)target->getLevel() > GetSpellProto()->EffectBasePoints[0]+1 + int32(p_caster->getLevel() - GetSpellProto()->spellLevel))
                            return SPELL_FAILED_HIGHLEVEL;
                        else if(target->GetTypeId() == TYPEID_UNIT)
                        {
                            Creature* c =  castPtr<Creature>(target);
                            if (c&&c->GetCreatureData()&&c->GetCreatureData()->Rank >ELITE_ELITE)
                                return SPELL_FAILED_HIGHLEVEL;
                        }
                    }
                }
            }

            // scripted spell stuff
            switch(GetSpellProto()->Id)
            {
            case 603: //curse of doom, can't be casted on players
            case 30910:
            case 47867:
                {
                    if(target->IsPlayer())
                        return SPELL_FAILED_TARGET_IS_PLAYER;
                }break;

            case 13907:
                {
                    if (!target || target->IsPlayer() || target->GetCreatureType()!=TARGET_TYPE_DEMON )
                        return SPELL_FAILED_SPELL_UNAVAILABLE;
                }break;

                // disable spell
            case 25997: // Eye for an Eye
            case 38554: //Absorb Eye of Grillok
                {
                    // do not allow spell to be cast
                    return SPELL_FAILED_SPELL_UNAVAILABLE;
                }break;

                //These spells are NPC only.
            case 25166: //Call Glyphs of Warding
            case 38892: //Shadow Bolt
            case 40536: //Chain Lightning
            case 41078: //Shadow Blast
                {
                    if(u_caster->IsPlayer())
                        return SPELL_FAILED_BAD_TARGETS;
                }break;

            case 982: //Revive Pet
                {
                    Pet* pPet = p_caster->GetSummon();
                    if(pPet && !pPet->isDead())
                        return SPELL_FAILED_TARGET_NOT_DEAD;
                }break;
            }

            // if the target is not the unit caster and not the masters pet
            if(target != u_caster && !m_caster->IsPet())
            {

                /***********************************************************
                * Inface checks, these are checked in 2 ways
                * 1e way is check for damage type, as 3 is always ranged
                * 2e way is trough the data in the extraspell db
                *
                **********************************************************/

                /* burlex: units are always facing the target! */
                if(p_caster && GetSpellProto()->FacingCasterFlags)
                {
                    if(!p_caster->isTargetInFront(target))
                        return SPELL_FAILED_UNIT_NOT_INFRONT;
                    if(GetSpellProto()->reqCasterIsBehindTarget() && (GetSpellProto()->Id != SPELL_RANGED_THROW) &&
                        !p_caster->isInBackOfTarget(target))
                        return SPELL_FAILED_NOT_BEHIND;
                }
            }

            // if target is already skinned, don't let it be skinned again
            if( GetSpellProto()->Effect[0] == SPELL_EFFECT_SKINNING) // skinning
                if(target->IsUnit() && (castPtr<Creature>(target)->Skinned) )
                    return SPELL_FAILED_TARGET_UNSKINNABLE;

            // target 39 is fishing, all fishing spells are handled
            if( GetSpellProto()->EffectImplicitTargetA[0] == 39 )
            {
                uint32 entry = GetSpellProto()->EffectMiscValue[0];
                if(entry == GO_FISHING_BOBBER)
                {
                    float px=u_caster->GetPositionX();
                    float py=u_caster->GetPositionY();
                    float orient = m_caster->GetOrientation();
                    float posx = 0,posy = 0,posz = 0;
                    float co = cos(orient);
                    float si = sin(orient);
                    MapMgr* map = m_caster->GetMapMgr();

                    float r;
                    for(r=20; r>10; r--)
                    {
                        posx = px + r * co;
                        posy = py + r * si;
                        posz = map->GetWaterHeight(posx,posy, NO_WATER_HEIGHT);
                        if(posz > map->GetLandHeight(posx,posy))//water
                            break;
                    }
                    if(r<=10)
                        return SPELL_FAILED_NOT_FISHABLE;

                    // if we are already fishing, dont cast it again
                    if(p_caster->GetSummonedObject())
                        if(p_caster->GetSummonedObject()->GetEntry() == GO_FISHING_BOBBER)
                            return SPELL_FAILED_SPELL_IN_PROGRESS;
                }
            }

            if( p_caster != NULL )
            {
                if( GetSpellProto()->NameHash == SPELL_HASH_GOUGE )// Gouge
                    if(!target->isTargetInFront(p_caster))
                        return SPELL_FAILED_NOT_INFRONT;

                if( GetSpellProto()->Category==1131)//Hammer of wrath, requires target to have 20- % of hp
                {
                    if(target->GetUInt32Value(UNIT_FIELD_HEALTH) == 0)
                        return SPELL_FAILED_BAD_TARGETS;

                    if(target->GetUInt32Value(UNIT_FIELD_MAXHEALTH)/target->GetUInt32Value(UNIT_FIELD_HEALTH)<5)
                         return SPELL_FAILED_BAD_TARGETS;
                }
                else if( GetSpellProto()->NameHash == SPELL_HASH_CONFLAGRATE)//Conflagrate, requires immolation spell on victim
                {
                    if(!target->HasAurasOfNameHashWithCaster(SPELL_HASH_IMMOLATION, NULL))
                        return SPELL_FAILED_BAD_TARGETS;
                }

                if( GetSpellProto()->NameHash == SPELL_HASH_ENVENOM )
                {
                    if( !target->m_AuraInterface.HasAuraVisual(5100) )
                        return SPELL_FAILED_BAD_TARGETS ;
                }

                if(target->GetDispelImmunity(GetSpellProto()->DispelType))
                    return SPELL_FAILED_PREVENTED_BY_MECHANIC-1;            // hackfix - burlex
            }

            // if we're replacing a higher rank, deny it
            if(IsAuraApplyingSpell())
            {
                AuraCheckResponse acr = target->m_AuraInterface.AuraCheck(m_spellInfo, m_caster->GetGUID());
                if( acr.Error == AURA_CHECK_RESULT_HIGHER_BUFF_PRESENT )
                    return SPELL_FAILED_AURA_BOUNCED;
            }

            //check if we are trying to stealth or turn invisible but it is not allowed right now
            if( IsStealthSpell() || IsInvisibilitySpell() )
            {
                //if we have Faerie Fire, we cannot stealth or turn invisible
                if( u_caster->m_AuraInterface.HasNegativeAuraWithNameHash( SPELL_HASH_FAERIE_FIRE ) || u_caster->m_AuraInterface.HasNegativeAuraWithNameHash( SPELL_HASH_FAERIE_FIRE__FERAL_ ) )
                    return SPELL_FAILED_SPELL_UNAVAILABLE;
            }

            if( target->IsPlayer() )
            {
                switch( GetSpellProto()->NameHash )
                {
                case SPELL_HASH_DIVINE_PROTECTION:
                case SPELL_HASH_DIVINE_SHIELD:
                case SPELL_HASH_HAND_OF_PROTECTION:
                    {
                        if( castPtr<Player>(target)->HasAura(25771) ) // Forbearance
                            return SPELL_FAILED_DAMAGE_IMMUNE;

                        if( !castPtr<Player>(target)->mAvengingWrath )
                            return SPELL_FAILED_DAMAGE_IMMUNE;
                    }break;

                case SPELL_HASH_AVENGING_WRATH:
                    {
                        if( !castPtr<Player>(target)->mAvengingWrath )
                            return SPELL_FAILED_DAMAGE_IMMUNE;
                    }break;

                case SPELL_HASH_ICE_BLOCK:
                    {
                        if( castPtr<Player>(target)->HasAura(41425) ) // Hypothermia
                            return SPELL_FAILED_DAMAGE_IMMUNE;

                    }break;

                case SPELL_HASH_POWER_WORD__SHIELD:
                    {
                        if( castPtr<Player>(target)->HasAura(6788) ) // Weakened Soul
                            return SPELL_FAILED_DAMAGE_IMMUNE;
                    }break;

                case SPELL_HASH_FIRST_AID:
                    {
                        if( castPtr<Player>(target)->HasAura(11196) ) // Recently Bandaged
                            return SPELL_FAILED_DAMAGE_IMMUNE;
                    }break;
                case SPELL_HASH_BLOODLUST:
                    {
                        if( castPtr<Player>(target)->HasAurasOfNameHashWithCaster(SPELL_HASH_SATED, NULL) )
                            return SPELL_FAILED_DAMAGE_IMMUNE;
                    }break;
                }
            }

            if (GetSpellProto()->MechanicsType == 16 && target->HasAura(11196))
                return SPELL_FAILED_DAMAGE_IMMUNE;
        }
    }

    // Special State Checks (for creatures & players)
    if( u_caster )
    {
        if( u_caster->SchoolCastPrevent[GetSpellProto()->School] )
        {
            uint32 now_ = getMSTime();
            if( now_ < u_caster->SchoolCastPrevent[GetSpellProto()->School] )
                return SPELL_FAILED_SILENCED;
        }

        if( u_caster->m_silenced && GetSpellProto()->School != NORMAL_DAMAGE )
        {
                // HACK FIX
                switch( GetSpellProto()->NameHash )
                {
                    case SPELL_HASH_ICE_BLOCK: //Ice Block
                    case 0x9840A1A6: //Divine Shield
                        break;

                    case 0x3DFA70E5: //Will of the Forsaken
                        {
                            if( u_caster->m_special_state & ( UNIT_STATE_FEAR | UNIT_STATE_CHARM | UNIT_STATE_SLEEP ) )
                                break;
                        }break;

                    case 0xF60291F4: //Death Wish
                    case 0x19700707: //Berserker Rage
                        {
                            if( u_caster->m_special_state & UNIT_STATE_FEAR )
                                break;
                        }break;

                    // {Insignia|Medallion} of the {Horde|Alliance}
                    case 0xC7C45478: //Immune Movement Impairment and Loss of Control
                    case SPELL_HASH_PVP_TRINKET: // insignia of the alliance/horde 2.4.3
                    case SPELL_HASH_EVERY_MAN_FOR_HIMSELF:
                    case SPELL_HASH_DISPERSION:
                        {
                            break;
                        }

                    case 0xCD4CDF55: // Barksin
                    { // This spell is usable while stunned, frozen, incapacitated, feared or asleep.  Lasts 12 sec.
                        if( u_caster->m_special_state & ( UNIT_STATE_STUN | UNIT_STATE_FEAR | UNIT_STATE_SLEEP ) ) // Uh, what unit_state is Frozen? (freezing trap...)
                            break;
                    }

                    default:
                            return SPELL_FAILED_SILENCED;
                }
        }

        if(target != NULL) /* -Supalosa- Shouldn't this be handled on Spell Apply? */
        {
            for( int i = 0; i < 3; i++ ) // if is going to cast a spell that breaks stun remove stun auras, looks a bit hacky but is the best way i can find
            {
                if( GetSpellProto()->EffectApplyAuraName[i] == SPELL_AURA_MECHANIC_IMMUNITY )
                    target->m_AuraInterface.RemoveAllAurasByMechanic( GetSpellProto()->EffectMiscValue[i] , -1 , true );
            }
        }

        if( u_caster->IsPacified() && GetSpellProto()->School == NORMAL_DAMAGE ) // only affects physical damage
        {
            // HACK FIX
            switch( GetSpellProto()->NameHash )
            {
            case SPELL_HASH_ICE_BLOCK: //Ice Block
            case 0x9840A1A6: //Divine Shield
            case 0x3DFA70E5: //Will of the Forsaken
                {
                    if( u_caster->m_special_state & (UNIT_STATE_FEAR | UNIT_STATE_CHARM | UNIT_STATE_SLEEP))
                        break;
                }break;
            case SPELL_HASH_DISPERSION:
            case SPELL_HASH_PVP_TRINKET: // insignia of the alliance/horde 2.4.3
            case SPELL_HASH_EVERY_MAN_FOR_HIMSELF:
                break;

            default:
                return SPELL_FAILED_PACIFIED;
            }
        }

        if( u_caster->IsStunned() || u_caster->IsFeared())
        {
             //HACK FIX
            switch( GetSpellProto()->NameHash )
            {
            case SPELL_HASH_HAND_OF_FREEDOM:
                {
                    if( !u_caster->HasDummyAura(SPELL_HASH_DIVINE_PURPOSE) )
                        return SPELL_FAILED_STUNNED;
                }break;
            case SPELL_HASH_ICE_BLOCK: //Ice Block
            case SPELL_HASH_DIVINE_SHIELD: //Divine Shield
            case SPELL_HASH_DIVINE_PROTECTION: //Divine Protection
            case 0xCD4CDF55: //Barkskin
                break;
                /* -Supalosa- For some reason, being charmed or sleep'd is counted as 'Stunned'.
                Check it: http://www.wowhead.com/?spell=700 */

            case 0xC7C45478: /* Immune Movement Impairment and Loss of Control (PvP Trinkets) */
                break;

            case 0x3DFA70E5: /* Will of the Forsaken (Undead Racial) */
                break;

            case SPELL_HASH_PVP_TRINKET: // insignia of the alliance/horde 2.4.3*/
            case SPELL_HASH_EVERY_MAN_FOR_HIMSELF:
                break;

            case SPELL_HASH_BLINK:
                break;

            case SPELL_HASH_DISPERSION:
                break;

            default:
                {
                    if(u_caster->IsStunned() && !GetSpellProto()->isUsableWhileStunned()
                        || u_caster->IsFeared() && !GetSpellProto()->isAvailableWhileFeared())
                        return SPELL_FAILED_STUNNED;
                }
            }
        }

        if(u_caster->GetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT) > 0)
        {
            SpellEntry * t_spellInfo = (u_caster->GetCurrentSpell() ? u_caster->GetCurrentSpell()->m_spellInfo : NULL);

            if(!t_spellInfo || !m_triggeredSpell)
                return SPELL_FAILED_SPELL_IN_PROGRESS;
            else if (t_spellInfo)
            {
                if(
                    t_spellInfo->EffectTriggerSpell[0] != GetSpellProto()->Id &&
                    t_spellInfo->EffectTriggerSpell[1] != GetSpellProto()->Id &&
                    t_spellInfo->EffectTriggerSpell[2] != GetSpellProto()->Id)
                {
                    return SPELL_FAILED_SPELL_IN_PROGRESS;
                }
            }
        }
    }

    // no problems found, so we must be ok
    return SPELL_CANCAST_OK;
}

void Spell::RemoveItems()
{
    // Item Charges & Used Item Removal
    if(i_caster)
    {
        // Stackable Item -> remove 1 from stack
        if(i_caster->GetUInt32Value(ITEM_FIELD_STACK_COUNT) > 1)
        {
            i_caster->ModUnsigned32Value(ITEM_FIELD_STACK_COUNT, -1);
            i_caster->m_isDirty = true;
        }
        // Expendable Item
        else if(i_caster->GetProto()->Spells[0].Charges < 0
             || i_caster->GetProto()->Spells[1].Charges == -1) // hackfix for healthstones/mana gems/depleted items
        {
            // if item has charges remaining -> remove 1 charge
            if(((int32)i_caster->GetUInt32Value(ITEM_FIELD_SPELL_CHARGES)) < -1)
            {
                i_caster->ModSignedInt32Value(ITEM_FIELD_SPELL_CHARGES, 1);
                i_caster->m_isDirty = true;
            }
            // if item has no charges remaining -> delete item
            else
            {
                if(i_caster->GetOwner()) // wtf?
                {
                    i_caster->GetOwner()->GetItemInterface()->SafeFullRemoveItemByGuid(i_caster->GetGUID());
                    i_caster = NULL;
                }
            }
        }
        // Non-Expendable Item -> remove 1 charge
        else if(i_caster->GetProto()->Spells[0].Charges > 0)
        {
            i_caster->ModSignedInt32Value(ITEM_FIELD_SPELL_CHARGES, -1);
            i_caster->m_isDirty = true;
        }
    }

    // Reagent Removal
    for(uint32 i=0; i<8 ;++i)
    {
        if( p_caster && GetSpellProto()->Reagent[i] && !p_caster->NoReagentCost)
        {
            p_caster->GetItemInterface()->RemoveItemAmt_ProtectPointer(GetSpellProto()->Reagent[i], GetSpellProto()->ReagentCount[i], &i_caster);
        }
    }
}

int32 Spell::CalculateEffect(uint32 i, Unit* target, int32 &bonusPoints)
{
    int32 value = GetSpellProto()->CalculateSpellPoints(i, u_caster ? u_caster->getLevel() : 0, p_caster ? p_caster->m_comboPoints : 0);
    if( p_caster != NULL )
    {
        SpellOverrideMap::iterator itr = p_caster->mSpellOverrideMap.find(GetSpellProto()->Id);
        if(itr != p_caster->mSpellOverrideMap.end())
        {
            ScriptOverrideList::iterator itrSO;
            for(itrSO = itr->second->begin(); itrSO != itr->second->end(); itrSO++)
                value += RandomUInt((*itrSO)->damage);
        }
    }

    if( u_caster != NULL )
    {
        int32 spell_flat_modifers = 0, spell_pct_modifers = 0;
        u_caster->SM_FIValue(SMT_MISC_EFFECT, &spell_flat_modifers,GetSpellProto()->SpellGroupType);
        u_caster->SM_FIValue(SMT_MISC_EFFECT, &spell_pct_modifers, GetSpellProto()->SpellGroupType);

        if( i == 0 )
        {
            u_caster->SM_FIValue(SMT_FIRST_EFFECT_BONUS,&spell_flat_modifers,GetSpellProto()->SpellGroupType);
            u_caster->SM_FIValue(SMT_FIRST_EFFECT_BONUS,&spell_pct_modifers,GetSpellProto()->SpellGroupType);
        }
        else if( i == 1 )
        {
            u_caster->SM_FIValue(SMT_SECOND_EFFECT_BONUS,&spell_flat_modifers,GetSpellProto()->SpellGroupType);
            u_caster->SM_FIValue(SMT_SECOND_EFFECT_BONUS,&spell_pct_modifers,GetSpellProto()->SpellGroupType);
        }

        if( ( i == 2 ) || ( i == 1 && GetSpellProto()->Effect[2] == 0 ) || ( i == 0 && GetSpellProto()->Effect[1] == 0 && GetSpellProto()->Effect[2] == 0 ) )
        {
            u_caster->SM_FIValue(SMT_LAST_EFFECT_BONUS,&spell_flat_modifers,GetSpellProto()->SpellGroupType);
            u_caster->SM_FIValue(SMT_LAST_EFFECT_BONUS,&spell_pct_modifers,GetSpellProto()->SpellGroupType);
        }
        value += float2int32(value * (float)(spell_pct_modifers / 100.0f)) + spell_flat_modifers;
    }

    return value;
}

void Spell::HandleTeleport(uint32 id, Unit* Target)
{
    if(Target == NULL || Target->GetTypeId() != TYPEID_PLAYER || !id || id == 1)
        return;

    Player* pTarget = castPtr<Player>( Target );

    uint32 mapid;
    float x,y,z,o;

    TeleportCoords* TC = TeleportCoordStorage.LookupEntry(id);
    if(TC == NULL)
    {
        switch(id)
        {
    /*  case :
            {
                mapid = ;
                x = f;
                y = f;
                z = f;
                o = 0.0f;
            }break;*/
        case 556: // Hearthstone effects.
        case 8690:
        case 39937:
            {
                mapid = pTarget->GetBindMapId();
                x = pTarget->GetBindPositionX();
                y = pTarget->GetBindPositionY();
                z = pTarget->GetBindPositionZ();
                o = pTarget->GetOrientation();
            }break;
        case 59901: // Portal Effect: Caverns Of Time
            {
                mapid = 1;
                x = -8164.8f;
                y = -4768.5f;
                z = 34.3f;
                o = 0.0f;
            }break;
        case 61419: // Portal Effect: The purple parlor
            {
                mapid = 571;
                x = 5848.48f;
                y = 853.706f;
                z = 843.182f;
                o = 0.0f;
            }break;
        case 61420: // Portal Effect: Violet Citadel
            {
                mapid = 571;
                x = 5819.26f;
                y = 829.774f;
                z = 680.22f;
                o = 0.0f;
            }break;

        default:
            {
                if(m_targets.m_destX && m_targets.m_destY)
                {
                    mapid = pTarget->GetMapId();
                    x = m_targets.m_destX;
                    y = m_targets.m_destY;
                    z = m_targets.m_destZ;
                    o = pTarget->GetOrientation();
                }
                else
                {
                    sLog.outDebug("Unknown teleport spell: %u", id);
                    return;
                }
            }break;
        }
    }
    else
    {
        mapid = TC->mapId;
        x = TC->x;
        y = TC->y;
        z = TC->z;
        o = TC->o;
    }

    pTarget->EventAttackStop();
    pTarget->SetSelection(NULL);
    pTarget->DelaySpeedHack(5000);

    // We use a teleport event on this one. Reason being because of UpdateCellActivity,
    // the game object set of the updater thread WILL Get messed up if we teleport from a gameobject caster.
    if(!sEventMgr.HasEvent(pTarget, EVENT_PLAYER_TELEPORT))
        sEventMgr.AddEvent(pTarget, &Player::EventTeleport, mapid, x, y, z, o, EVENT_PLAYER_TELEPORT, 1, 1,EVENT_FLAG_DO_NOT_EXECUTE_IN_WORLD_CONTEXT);
}

void Spell::CreateItem(uint32 itemId)
{
    if( !itemId )
        return;

    Player*         pUnit = castPtr<Player>( m_caster );
    Item*           newItem = NULL;
    Item*           add = NULL;
    SlotResult      slotresult;
    ItemPrototype*  m_itemProto;

    m_itemProto = ItemPrototypeStorage.LookupEntry( itemId );
    if( m_itemProto == NULL )
        return;

    if (pUnit->GetItemInterface()->CanReceiveItem(m_itemProto, 1, NULL))
    {
        SendCastResult(SPELL_FAILED_TOO_MANY_OF_ITEM);
        return;
    }

    add = pUnit->GetItemInterface()->FindItemLessMax(itemId, 1, false);
    if (!add)
    {
        slotresult = pUnit->GetItemInterface()->FindFreeInventorySlot(m_itemProto);
        if(!slotresult.Result)
        {
             SendCastResult(SPELL_FAILED_TOO_MANY_OF_ITEM);
             return;
        }

        newItem = objmgr.CreateItem(itemId, pUnit);
        AddItemResult result = pUnit->GetItemInterface()->SafeAddItem(newItem, slotresult.ContainerSlot, slotresult.Slot);
        if(!result)
        {
            newItem->Destruct();
            newItem = NULL;
            return;
        }

        newItem->SetUInt64Value(ITEM_FIELD_CREATOR,m_caster->GetGUID());
        newItem->SetUInt32Value(ITEM_FIELD_STACK_COUNT, damage);
        p_caster->GetSession()->SendItemPushResult(newItem,true,false,true,true,slotresult.ContainerSlot,slotresult.Slot,1);
        newItem->m_isDirty = true;

    }
    else
    {
        add->SetUInt32Value(ITEM_FIELD_STACK_COUNT,add->GetUInt32Value(ITEM_FIELD_STACK_COUNT) + damage);
        p_caster->GetSession()->SendItemPushResult(add,true,false,true,false,p_caster->GetItemInterface()->GetBagSlotByGuid(add->GetGUID()),0xFFFFFFFF,1);
        add->m_isDirty = true;
    }
}

void Spell::SendHealSpellOnPlayer( WorldObject* caster, WorldObject* target, uint32 dmg, bool critical, uint32 overheal, uint32 spellid)
{
    if( caster == NULL || target == NULL || !target->IsPlayer())
        return;

    WorldPacket data(SMSG_SPELLHEALLOG, 34);
    data << target->GetGUID();
    data << caster->GetGUID();
    data << uint32(spellid);
    data << uint32(dmg);
    data << uint32(overheal);
    data << uint32(0);
    data << uint8(critical ? 1 : 0);
    data << uint8(0);
    caster->SendMessageToSet(&data, true);
}

void Spell::SendHealManaSpellOnPlayer(WorldObject* caster, WorldObject* target, uint32 dmg, uint32 powertype, uint32 spellid)
{
    if( caster == NULL || target == NULL)
        return;

    WorldPacket data(SMSG_SPELLENERGIZELOG, 29);
    data.append(target->GetGUID());
    data.append(caster->GetGUID());
    data << uint32(spellid);
    data << uint32(powertype);
    data << uint32(dmg);
    caster->SendMessageToSet(&data, true);
}

void Spell::Heal(uint8 effIndex, int32 amount)
{
    if( unitTarget == NULL || !unitTarget->isAlive() )
        return;

    if( p_caster != NULL )
        p_caster->last_heal_spell=m_spellInfo;

    //self healing shouldn't flag himself
    if( p_caster != NULL && p_caster->GetGUID() != unitTarget->GetGUID() )
    {
        // Healing a flagged target will flag you.
        if( unitTarget->IsPvPFlagged() )
        {
            if( !p_caster->IsPvPFlagged() )
                p_caster->PvPToggle();
            else
                p_caster->SetPvPFlag();
        }
    }

    //Make it critical
    bool critical = false;
    float critchance = 0;
    //int32 bonus = 0;
    if( u_caster != NULL )
    {
        // All calculations are done in getspellbonusdamage
        // 3.0.2 Spellpower change: In order to keep the effective amount healed for a given spell the same, we’d expect the original coefficients to be multiplied by 1/0.532 or 1.88.
        amount = u_caster->GetSpellBonusDamage(unitTarget, m_spellInfo, effIndex, amount, true);
        if(!GetSpellProto()->isUncrittableSpell())
        {
            critchance = u_caster->spellcritperc + u_caster->SpellCritChanceSchool[GetSpellProto()->School];
            if( GetSpellProto()->SpellGroupType )
            {
                u_caster->SM_FFValue(SMT_CRITICAL, &critchance, GetSpellProto()->SpellGroupType);
                u_caster->SM_PFValue(SMT_CRITICAL, &critchance, GetSpellProto()->SpellGroupType);
            }
        }

        if(critical = Rand(critchance))
        {
            int32 critical_bonus = 100;
            if( GetSpellProto()->SpellGroupType )
                u_caster->SM_FIValue(SMT_CRITICAL_DAMAGE, &critical_bonus, GetSpellProto()->SpellGroupType );

            if( critical_bonus > 0 )
            {
                // the bonuses are halved by 50% (funky blizzard math :S)
                float b = ( ( float(critical_bonus) / 2.0f ) / 100.0f );
                amount += float2int32( float(amount) * b );
            }
        }
    }

    if(amount < 0)
        amount = 0;

    uint32 overheal = 0;
    uint32 curHealth = unitTarget->GetUInt32Value(UNIT_FIELD_HEALTH);
    uint32 maxHealth = unitTarget->GetUInt32Value(UNIT_FIELD_MAXHEALTH);
    if((curHealth + amount) >= maxHealth)
    {
        unitTarget->SetUInt32Value(UNIT_FIELD_HEALTH, maxHealth);
        overheal = curHealth + amount - maxHealth;
    } else unitTarget->ModUnsigned32Value(UNIT_FIELD_HEALTH, amount);

    if( m_caster )
        SendHealSpellOnPlayer( m_caster, unitTarget, amount, critical, overheal, (m_triggeredSpellId ? m_triggeredSpellId : GetSpellProto()->Id));

    if( p_caster != NULL )
    {
        //Beacon of Light
        Player *HealTarget;
        uint32 beaconoverheal;
        if(p_caster->GetGroup() && p_caster->IsInWorld())
        {
            std::map<WoWGuid, WoWGuid> beaconmap = p_caster->GetGroup()->m_BeaconOfLightTargets;
            if(beaconmap.size())
            {
                for(std::map<WoWGuid, WoWGuid>::iterator itr = beaconmap.begin(); itr != beaconmap.end(); itr++)
                {
                    if((HealTarget = p_caster->GetMapMgr()->GetPlayer(itr->first)) != NULL && HealTarget->IsInWorld())
                    {
                        if(p_caster->GetGUID() != itr->second)
                            continue;
                        if(HealTarget->GetGUID() == unitTarget->GetGUID())  // don't heal our target again.
                            continue;
                        if(p_caster->DuelingWith == HealTarget)
                            continue;

                        beaconoverheal = p_caster->Heal(HealTarget, GetSpellProto()->Id, amount);
                        p_caster->m_bgScore.HealingDone += amount - beaconoverheal;
                    } else beaconmap.erase(itr->first);
                }
            }
        }

        p_caster->m_bgScore.HealingDone += amount - overheal;
        if( p_caster->m_bg != NULL )
            p_caster->m_bg->UpdatePvPData();

        p_caster->m_casted_amount[GetSpellProto()->School] = amount;
    }

    // add threat
    if( u_caster != NULL )
    {
        uint32 base_threat = GetBaseThreat(amount);
        int count = 0;
        Unit* unit;
        std::vector<Unit* > target_threat;
        if(base_threat)
        {
            target_threat.reserve(u_caster->GetInRangeCount()); // this helps speed

            for(std::unordered_set<WorldObject* >::iterator itr = u_caster->GetInRangeSetBegin(); itr != u_caster->GetInRangeSetEnd(); itr++)
            {
                if((*itr)->GetTypeId() != TYPEID_UNIT)
                    continue;
                unit = castPtr<Unit>((*itr));
                if(unit->GetAIInterface()->GetNextTarget() == unitTarget)
                {
                    target_threat.push_back(unit);
                    ++count;
                }
            }
            if(count == 0)
                count = 1;  // division against 0 protection
            /*
            When a tank hold multiple mobs, the threat of a heal on the tank will be split between all the mobs.
            The exact formula is not yet known, but it is more than the Threat/number of mobs.
            So if a tank holds 5 mobs and receives a heal, the threat on each mob will be less than Threat(heal)/5.
            Current speculation is Threat(heal)/(num of mobs *2)
            */
            uint32 threat = base_threat / (count * 2);

            for(std::vector<Unit* >::iterator itr = target_threat.begin(); itr != target_threat.end(); itr++)
            {
                // for now we'll just use heal amount as threat.. we'll prolly need a formula though
                castPtr<Unit>(*itr)->GetAIInterface()->HealReaction( u_caster, unitTarget, threat, m_spellInfo );
            }
        }

        if(unitTarget->IsInWorld() && u_caster->IsInWorld())
            u_caster->CombatStatus.WeHealed(unitTarget);
    }
}

void Spell::DetermineSkillUp(uint32 skillid,uint32 targetlevel, uint32 multiplicator)
{
    if(p_caster == NULL)
        return;
    if(p_caster->GetSkillUpChance(skillid)<0.01f)
        return;//to preven getting higher skill than max

    int32 diff = abs(int(p_caster->_GetSkillLineCurrent(skillid,false)/5 - targetlevel));
    float chance = ( diff <=5  ? 95.0f : diff <=10 ? 66.0f : diff <=15 ? 33.0f : 0.0f );
    if( Rand(int32(chance * sWorld.getRate(RATE_SKILLCHANCE) * (multiplicator?multiplicator:1))))
        p_caster->_AdvanceSkillLine(skillid, float2int32(1.0f * sWorld.getRate(RATE_SKILLRATE)));
}

void Spell::DetermineSkillUp(uint32 skillid)
{
    //This code is wrong for creating items and disenchanting.
    if(p_caster == NULL)
        return;
    float chance = 0.0f;
    SkillLineSpell* skill = objmgr.GetSpellSkill(GetSpellProto()->Id);
    if( skill != NULL && castPtr<Player>( m_caster )->_HasSkillLine( skill->skilline ) )
    {
        uint32 amt = castPtr<Player>( m_caster )->_GetSkillLineCurrent( skill->skilline, false );
        uint32 max = castPtr<Player>( m_caster )->_GetSkillLineMax( skill->skilline );
        if( amt >= max )
            return;
        if( amt >= skill->RankMax ) //grey
            chance = 0.0f;
        else if( ( amt >= ( ( ( skill->RankMax - skill->RankMin) / 2 ) + skill->RankMin ) ) ) //green
            chance = 33.0f;
        else if( amt >= skill->RankMin ) //yellow
            chance = 66.0f;
        else //orange
            chance=100.0f;
    }
    if(Rand(chance*sWorld.getRate(RATE_SKILLCHANCE)))
        p_caster->_AdvanceSkillLine(skillid, float2int32( 1.0f * sWorld.getRate(RATE_SKILLRATE)));
}

bool Spell::Reflect(Unit* refunit)
{
    uint32 refspellid = 0;
    bool canreflect = false;
//  bool remove = false;

    if( m_reflectedParent != NULL || m_caster == refunit )
        return false;

    // if the spell to reflect is a reflect spell, do nothing.
    for(int i=0; i<3; i++)
    {
        if( GetSpellProto()->Effect[i] == 6 && (GetSpellProto()->EffectApplyAuraName[i] == 74 || GetSpellProto()->EffectApplyAuraName[i] == 28))
            return false;
    }

    if( !refspellid || !canreflect )
        return false;

    Spell* spell = new Spell(refunit, m_spellInfo, true, NULL);
    SpellCastTargets targets;
    targets.m_unitTarget = m_caster->GetGUID();
    spell->m_reflectedParent = this;
    return spell->prepare(&targets) == SPELL_CANCAST_OK;
}

void ApplyDiminishingReturnTimer(int32 * Duration, Unit* Target, SpellEntry * spell)
{
    uint32 status = GetDiminishingGroup(spell->NameHash);
    uint32 Grp = status & 0xFFFF;   // other bytes are if apply to pvp
    uint32 PvE = (status >> 16) & 0xFFFF;

    // Make sure we have a group
    if(Grp == 0xFFFF) return;

    // Check if we don't apply to pve
    if(!PvE && Target->GetTypeId() != TYPEID_PLAYER && !Target->IsPet())
        return;

    assert(Grp < DIMINISH_GROUPS);

    // TODO: check for spells that should do this
    float Qduration = float(*Duration);

    switch(Target->m_diminishCount[Grp])
    {
    case 0: // Full effect
        if (Target->IsPlayer() && Qduration > 10000)
        {
            Qduration = 10000;
        }
        break;

    case 1: // Reduced by 50%
        Qduration *= 0.5f;
        if (Target->IsPlayer() && Qduration > 5000)
        {
            Qduration = 5000;
        }
        break;

    case 2: // Reduced by 75%
        Qduration *= 0.25f;
        if (Target->IsPlayer() && Qduration > 2500)
        {
            Qduration = 2500;
        }
        break;

    default:// Target immune to spell
        {
            *Duration = 0;
            return;
        }break;
    }

    // Convert back
    *Duration = FL2UINT(Qduration);

    // Reset the diminishing return counter, and add to the aura count (we don't decrease the timer till we
    // have no auras of this type left.
    ++Target->m_diminishAuraCount[Grp];
    ++Target->m_diminishCount[Grp];
}

void UnapplyDiminishingReturnTimer(Unit* Target, SpellEntry * spell)
{
    uint32 status = GetDiminishingGroup(spell->NameHash);
    uint32 Grp = status & 0xFFFF;   // other bytes are if apply to pvp
    uint32 PvE = (status >> 16) & 0xFFFF;

    // Make sure we have a group
    if(Grp == 0xFFFF) return;

    // Check if we don't apply to pve
    if(!PvE && Target->GetTypeId() != TYPEID_PLAYER && !Target->IsPet())
        return;

    assert(Grp < DIMINISH_GROUPS);

    Target->m_diminishAuraCount[Grp]--;

    // start timer decrease
    if(!Target->m_diminishAuraCount[Grp])
    {
        Target->SetDiminishTimer(Grp);
    }
}

/// Calculate the Diminishing Group. This is based on a name hash.
/// this off course is very hacky, but as its made done in a proper way
/// I leave it here.
uint32 GetDiminishingGroup(uint32 NameHash)
{
    int32 grp = -1;
    bool pve = false;

    switch(NameHash)
    {
    case SPELL_HASH_CYCLONE:
    case SPELL_HASH_BLIND:
        grp = 0;
        pve = true;
        break;
    case SPELL_HASH_MIND_CONTROL:
        grp = 1;
        break;
    case SPELL_HASH_FEAR:
    case SPELL_HASH_PSYCHIC_SCREAM:
    case SPELL_HASH_HOWL_OF_TERROR:
    case SPELL_HASH_SEDUCTION:
        grp = 2;
        break;
    case SPELL_HASH_SAP:
    case SPELL_HASH_GOUGE:
    case SPELL_HASH_REPENTANCE:
    case SPELL_HASH_POLYMORPH:              // Polymorph
    case SPELL_HASH_POLYMORPH__CHICKEN:     // Chicken
    case SPELL_HASH_POLYMORPH__SHEEP:       // Good ol' sheep
        grp = 3;
        break;
    case SPELL_HASH_DEATH_COIL:
        grp = 4;
        break;
    case SPELL_HASH_KIDNEY_SHOT:
        grp = 5;
        pve = true;
        break;
    case SPELL_HASH_ENTRAPMENT:
        grp = 6;
        break;
    case SPELL_HASH_ENTANGLING_ROOTS:
    case SPELL_HASH_FROST_NOVA:
        grp = 7;
        break;
    case SPELL_HASH_FROSTBITE:
        grp = 8;
        break;
    case SPELL_HASH_HIBERNATE:
    case SPELL_HASH_WYVERN_STING:
    case SPELL_HASH_SLEEP:
    case SPELL_HASH_FROST_TRAP_AURA:
    case SPELL_HASH_FREEZING_TRAP_EFFECT:
        grp = 9;
        break;
    case SPELL_HASH_BASH:
    case SPELL_HASH_IMPACT:
    case SPELL_HASH_HAMMER_OF_JUSTICE:
    case SPELL_HASH_CHEAP_SHOT:
    case SPELL_HASH_SHADOWFURY:
    case SPELL_HASH_CHARGE_STUN:
    case SPELL_HASH_INTERCEPT:
    case SPELL_HASH_CONCUSSION_BLOW:
        grp = 10;
        pve = true;
        break;
    case SPELL_HASH_STONECLAW_STUN:
    case SPELL_HASH_STUN:
    case SPELL_HASH_BLACKOUT:
        grp = 11;
        pve = true;
        break;
    case SPELL_HASH_HEX:
        grp = 12;
        break;


        /*case SPELL_HASH_BANISH:               // Banish
        grp = 19;
        break;

        case SPELL_HASH_FREEZING_TRAP_EFFECT:   // Freezing Trap Effect
        grp = 20;
        break;

        case SPELL_HASH_SCARE_BEAST:            // Scare Beast
        grp = 21;
        break;

        case SPELL_HASH_ENSLAVE_DEMON:          // Enslave Demon
        grp = 22;
        break;
        case SPELL_HASH_SLEEP:                  // Sleep
        grp = 23;
        break;
        case SPELL_HASH_RIPOSTE:
        grp = 24;
        break;*/
    }
    uint32 ret;
    if( pve )
        ret = grp | (1 << 16);
    else
        ret = grp;

    return ret;
}

void Spell::_AddTarget(Unit* target, const uint32 effectid)
{
    // Check if we're in the current list already, and if so, don't readd us.
    if(ManagedTargets.find(target->GetGUID()) != ManagedTargets.end())
        return;

    bool found = false;
    // look for the target in the list already
    SpellTargetMap::iterator itr = TargetMap.find(target->GetGUID());
    if(found = (itr != TargetMap.end()))
        itr->second.EffectMask |= (1 << effectid);

    // setup struct
    SpellTarget tgt;
    tgt.Guid = target->GetGUID();

    if(m_spellInfo->speed > 0.0f)
    {
        // calculate spell incoming interval
        float dist = m_caster->CalcDistance(target);
        tgt.DestinationTime = int64(floor(dist / m_spellInfo->speed*1000.0f));
        if(tgt.DestinationTime+MSTimeToAddToTravel < 200)
            tgt.DestinationTime = 0;
        if (m_missileTravelTime == 0 || tgt.DestinationTime > m_missileTravelTime)
            m_missileTravelTime = tgt.DestinationTime;
        tgt.DestinationTime += MSTimeToAddToTravel;
    }
    else
        tgt.DestinationTime = 0;

    // work out hit result (always true if we are a GO)
    tgt.HitResult = (g_caster || (g_caster && g_caster->GetType() != GAMEOBJECT_TYPE_TRAP) ) ? SPELL_DID_HIT_SUCCESS : _DidHit(effectid, target, tgt.ReflectResult);

    // add to the list
    ManagedTargets.insert(std::make_pair(target->GetGUID(), tgt));
    if(!found)
        TargetMap.insert(std::make_pair(target->GetGUID(), tgt));

    // add counter
    if( tgt.HitResult == SPELL_DID_HIT_SUCCESS )
        ++m_hitTargetCount;
    else
        ++m_missTargetCount;
}

void Spell::_AddTargetForced(const WoWGuid& guid, const uint32 effectid)
{
    // Check if we're in the current list already, and if so, don't readd us.
    if(ManagedTargets.find(guid) != ManagedTargets.end())
        return;

    bool found = false;
    // look for the target in the list already
    SpellTargetMap::iterator itr = TargetMap.find(guid);
    if(found = (itr != TargetMap.end()))
        itr->second.EffectMask |= (1 << effectid);

    // setup struct
    SpellTarget tgt;
    tgt.Guid = guid;
    tgt.DestinationTime = 0;
    if(m_spellInfo->speed > 0.0f)
    {
        if(WorldObject* obj = m_caster->GetMapMgr()->_GetObject(guid)) // This should always happen.
        {
            // calculate spell incoming interval
            float dist = m_caster->CalcDistance(obj);
            tgt.DestinationTime = int64(floor(dist / m_spellInfo->speed*1000.0f));
            if(tgt.DestinationTime+MSTimeToAddToTravel < 200)
                tgt.DestinationTime = 0;
            if (m_missileTravelTime == 0 || tgt.DestinationTime > m_missileTravelTime)
                m_missileTravelTime = tgt.DestinationTime;
            tgt.DestinationTime += MSTimeToAddToTravel;
        }
    }
    tgt.EffectMask = (1 << effectid);
    tgt.HitResult = SPELL_DID_HIT_SUCCESS;

    // add to the list
    ManagedTargets.insert(std::make_pair(guid, tgt));
    if(!found)
        TargetMap.insert(std::make_pair(guid, tgt));

    // add counter
    ++m_hitTargetCount;
}

void Spell::DamageGosAround(uint32 i)
{
    uint32 spell_id = GetSpellProto()->Id;
    float r = GetRadius(i);
    r *= r;
    WorldObject* o;

    for (WorldObject::InRangeSet::iterator itr = m_caster->GetInRangeSetBegin(); itr != m_caster->GetInRangeSetEnd(); ++itr)
    {
        o = *itr;
        if (!o->IsGameObject())
            continue;
        if(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION && o->GetDistance2dSq(m_targets.m_destX, m_targets.m_destY) <= r )
        {
            castPtr<GameObject>(o)->TakeDamage(damage,m_caster,p_caster,spell_id);
            return;
        }
        else if(m_targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION && o->GetDistance2dSq(m_targets.m_srcX, m_targets.m_srcY) <= r)
        {
            castPtr<GameObject>(o)->TakeDamage(damage,m_caster,p_caster,spell_id);
            return;
        }
        else
        {
            if(o->GetDistance2dSq(m_caster->GetPositionX(), m_caster->GetPositionY()) <= r)
                castPtr<GameObject>(o)->TakeDamage(damage,m_caster,p_caster,spell_id);
        }
    }
}

bool Spell::CanHandleSpellEffect(uint32 i, uint32 namehash)
{
    if(!u_caster)
        return false;
    switch(i)
    {
    case 1:
        {
            switch(namehash)
            {
                case SPELL_HASH_FROSTBOLT:
                {
                    if(u_caster->HasDummyAura( SPELL_HASH_GLYPH_OF_FROSTBOLT ))
                        return false;
                }break;
            }break;
        }break;
    case 2:
        {
            switch(namehash)
            {
                case SPELL_HASH_FIREBALL:
                {
                    if(u_caster->HasDummyAura( SPELL_HASH_GLYPH_OF_FIREBALL ))
                        return false;
                }break;
            }break;
        }break;
    case 3:
        {
            switch(namehash)
            {
                case SPELL_HASH_BLAST_WAVE:
                {
                    if(u_caster->HasAura(62126))
                        return false;
                }break;     
                case SPELL_HASH_THUNDERSTORM:
                {
                    if(u_caster->HasAura(62132))
                        return false;
                }break;
            }break;
        }break;
    }
    return true;
}

bool Spell::UseMissileDelay()
{
    if(HasSpellEffect(SPELL_EFFECT_CHARGE) || HasSpellEffect(SPELL_EFFECT_JUMP_TO_TARGET) ||
        HasSpellEffect(SPELL_EFFECT_JUMP_TO_DESTIONATION) || HasSpellEffect(SPELL_EFFECT_TRACTOR_BEAM_FROM_DEST) || 
        HasSpellEffect(SPELL_EFFECT_PLAYER_PULL))
        return false;
    return true;
}


bool CanAgroHash(uint32 spellhashname)
{
    if (spellhashname == SPELL_HASH_HUNTER_S_MARK || spellhashname == SPELL_HASH_SAP || spellhashname == SPELL_HASH_EAGLE_EYE || spellhashname == SPELL_HASH_FAR_SIGHT )    //hunter's mark
        return false;
    return true;
}

bool IsCastedOnFriends(SpellEntry *sp)
{
    for( int frloop = 0; frloop < 3; frloop++ )
    {
        switch (sp->EffectImplicitTargetA[frloop])
        {
            case 1:     //EFF_TARGET_SELF
            case 4:
            case 5:     //EFF_TARGET_PET
            case 20:    //EFF_TARGET_ALL_PARTY_AROUND_CASTER
            case 21:    //EFF_TARGET_SINGLE_FRIEND
            case 27:    //EFF_TARGET_PET_MASTER
            case 30:    //EFF_TARGET_ALL_FRIENDLY_IN_AREA
            case 31:    //EFF_TARGET_ALL_TARGETABLE_AROUND_LOCATION_IN_RADIUS_OVER_TIME
            case 32:    //EFF_TARGET_MINION
            case 33:    //EFF_TARGET_ALL_PARTY_IN_AREA
            case 35:    //EFF_TARGET_SINGLE_PARTY
            case 37:    //EFF_TARGET_ALL_PARTY
            case 41:    //EFF_TARGET_TOTEM_EARTH
            case 42:    //EFF_TARGET_TOTEM_WATER
            case 43:    //EFF_TARGET_TOTEM_AIR
            case 44:    //EFF_TARGET_TOTEM_FIRE
            case 45:    //EFF_TARGET_CHAIN
            case 56:
            case 57:    //EFF_TARGET_PARTY_MEMBER
            case 61:    //EFF_TARGET_AREAEFFECT_PARTY_AND_CLASS
                return true;
        }

        switch (sp->EffectImplicitTargetB[frloop])
        {
            case 1:     //EFF_TARGET_SELF
            case 4:
            case 5:     //EFF_TARGET_PET
            case 20:    //EFF_TARGET_ALL_PARTY_AROUND_CASTER
            case 21:    //EFF_TARGET_SINGLE_FRIEND
            case 27:    //EFF_TARGET_PET_MASTER
            case 30:    //EFF_TARGET_ALL_FRIENDLY_IN_AREA
            case 31:    //EFF_TARGET_ALL_TARGETABLE_AROUND_LOCATION_IN_RADIUS_OVER_TIME
            case 32:    //EFF_TARGET_MINION
            case 33:    //EFF_TARGET_ALL_PARTY_IN_AREA
            case 35:    //EFF_TARGET_SINGLE_PARTY
            case 37:    //EFF_TARGET_ALL_PARTY
            case 41:    //EFF_TARGET_TOTEM_EARTH
            case 42:    //EFF_TARGET_TOTEM_WATER
            case 43:    //EFF_TARGET_TOTEM_AIR
            case 44:    //EFF_TARGET_TOTEM_FIRE
            case 45:    //EFF_TARGET_CHAIN
            case 56:
            case 57:    //EFF_TARGET_PARTY_MEMBER
            case 61:    //EFF_TARGET_AREAEFFECT_PARTY_AND_CLASS
                return true;
        }
    }
    return false;
}

bool IsCastedOnEnemies(SpellEntry *sp)
{
    for( int frloop = 0; frloop < 3; frloop++ )
    {
        switch (sp->EffectImplicitTargetA[frloop])
        {
            case 6:     //EFF_TARGET_SINGLE_ENEMY
            case 8:     //EFF_TARGET_ALL_TARGETABLE_AROUND_LOCATION_IN_RADIUS
            case 15:    //EFF_TARGET_ALL_ENEMY_IN_AREA
            case 16:    //EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT
            case 22:    //EFF_TARGET_ALL_ENEMIES_AROUND_CASTER
            case 24:    //EFF_TARGET_IN_FRONT_OF_CASTER
            case 28:    //EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED
            case 54:    //EFF_TARGET_TARGET_AT_ORIENTATION_TO_CASTER
            case 77:    //EFF_TARGET_SELECTED_ENEMY_CHANNELED
                return true;
        }

        switch (sp->EffectImplicitTargetB[frloop])
        {
            case 6:     //EFF_TARGET_SINGLE_ENEMY
            case 8:     //EFF_TARGET_ALL_TARGETABLE_AROUND_LOCATION_IN_RADIUS
            case 15:    //EFF_TARGET_ALL_ENEMY_IN_AREA
            case 16:    //EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT
            case 22:    //EFF_TARGET_ALL_ENEMIES_AROUND_CASTER
            case 24:    //EFF_TARGET_IN_FRONT_OF_CASTER
            case 28:    //EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED
            case 54:    //EFF_TARGET_TARGET_AT_ORIENTATION_TO_CASTER
            case 77:    //EFF_TARGET_SELECTED_ENEMY_CHANNELED
                return true;
        }
    }
    return false;
}

/************************************************************************/
/* IsDamagingSpell, this function seems slow, its only used rarely      */
/************************************************************************/
bool IsDamagingSpell(SpellEntry *sp)
{
    if( sp->NameHash == SPELL_HASH_MUTILATE )
        return true;

    for (uint32 i = 0; i < 3; i++)
    {
        switch (sp->Effect[i])
        {
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        case SPELL_EFFECT_ENVIRONMENTAL_DAMAGE:
        case SPELL_EFFECT_HEALTH_LEECH:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
        case SPELL_EFFECT_ADD_EXTRA_ATTACKS:
        case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_POWER_BURN:
        case SPELL_EFFECT_ATTACK:
        case SPELL_EFFECT_DUMMYMELEE:
            return true;

        case SPELL_EFFECT_APPLY_AURA:
        case SPELL_EFFECT_APPLY_AREA_AURA:
            {
                switch (sp->EffectApplyAuraName[i])
                {
                case SPELL_AURA_PERIODIC_DAMAGE://SPELL_AURA_PERIODIC_DAMAGE:
                case SPELL_AURA_PROC_TRIGGER_DAMAGE://SPELL_AURA_PROC_TRIGGER_DAMAGE:
                case SPELL_AURA_PERIODIC_LEECH://SPELL_AURA_PERIODIC_LEECH:
                case SPELL_AURA_PERIODIC_DAMAGE_PERCENT://SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                case SPELL_AURA_POWER_BURN_MANA://SPELL_AURA_POWER_BURN:
                    return true;
                }
            }
        }
    }
    return false;
}

bool IsHealingSpell(SpellEntry *sp)
{
    for(uint32 i = 0; i < 3; i++)
    {
        switch( sp->Effect[i] )
        {
        case SPELL_EFFECT_SCHOOL_DAMAGE:
            return false;
            break;

        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_HEALTH_FUNNEL:
        case SPELL_EFFECT_HEAL_MAX_HEALTH:
            return true;
            break;

        case SPELL_EFFECT_APPLY_AURA:
        case SPELL_EFFECT_APPLY_AREA_AURA:
            {
                switch( sp->EffectApplyAuraName[i] )
                {
                case SPELL_AURA_PERIODIC_HEAL:
                case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
                    return true;
                    break;

                case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
                    {
                        SpellEntry * triggered = dbcSpell.LookupEntry(sp->EffectTriggerSpell[i]);
                        if(triggered && triggered != sp && IsHealingSpell(triggered))
                            return true;
                    }break;
                }
            }break;
        }
    }

    //flash of light, holy light uses a scripted effect which is not necessarily a heal spell
    if( sp->NameHash == SPELL_HASH_HOLY_LIGHT || sp->NameHash == SPELL_HASH_FLASH_OF_LIGHT )
        return true;

    return false;
}

bool IsInrange(LocationVector & location, WorldObject* o, float square_r, float square_min)
{
    float r = o->GetDistanceSq(location);
    if(square_min && r < square_min)
        return false;
    return ( r<=square_r);
}

bool IsInrange(float x1,float y1, float z1, WorldObject* o,float square_r, float square_min)
{
    float r = o->GetDistanceSq(x1, y1, z1);
    if(square_min && r < square_min)
        return false;
    return ( r<=square_r);
}

bool IsInrange(float x1,float y1, float z1,float x2,float y2, float z2,float square_r, float square_min)
{
    float t;
    float r;
    t=x1-x2;
    r=t*t;
    t=y1-y2;
    r+=t*t;
    t=z1-z2;
    r+=t*t;
    if(square_min && r < square_min)
        return false;
    return ( r<=square_r);
}

bool IsInrange(WorldObject* o1,WorldObject* o2,float square_r, float square_min)
{
    return IsInrange(o1->GetPositionX(),o1->GetPositionY(),o1->GetPositionZ(),
        o2->GetPositionX(),o2->GetPositionY(),o2->GetPositionZ(),square_r,square_min);
}

bool TargetTypeCheck(WorldObject* obj,uint32 ReqCreatureTypeMask)
{
    if( !ReqCreatureTypeMask )
        return true;

    if( obj->GetTypeId() == TYPEID_UNIT )
    {
        Creature* cr = castPtr<Creature>(obj);
        CreatureData* inf = cr->GetCreatureData();
        if( inf == NULL || !( 1 << ( inf->Type - 1 ) & ReqCreatureTypeMask ) )
            return false;
    }
    else if(obj->IsPlayer() && !(UNIT_TYPE_HUMANOID_BIT & ReqCreatureTypeMask))
        return false;
    else return false;//omg, how in the hack did we cast it on a GO ? But who cares ?
    return true;
}

bool IsFlyingSpell(SpellEntry *sp)
{
    if( sp->EffectApplyAuraName[0] == 206 ||
        sp->EffectApplyAuraName[1] == 206 ||
        sp->EffectApplyAuraName[1] == 206 ||

        sp->EffectApplyAuraName[0] == 207 ||
        sp->EffectApplyAuraName[1] == 207 ||
        sp->EffectApplyAuraName[1] == 207 ||

        sp->EffectApplyAuraName[0] == 208 ||
        sp->EffectApplyAuraName[1] == 208 ||
        sp->EffectApplyAuraName[1] == 208 ||

        sp->NameHash == SPELL_HASH_SWIFT_FLIGHT_FORM ||
        sp->NameHash == SPELL_HASH_FLIGHT_FORM ||
        sp->NameHash == SPELL_HASH_MAGNIFICENT_FLYING_CARPET ||
        sp->NameHash == SPELL_HASH_FLYING_CARPET )
    {
        return true;
    }

    return false;
}

bool IsTargetingStealthed(SpellEntry *sp)
{
    if( sp->Id == 3600 )
        return false;

    if(     sp->EffectImplicitTargetA[0]==EFF_TARGET_INVISIBLE_OR_HIDDEN_ENEMIES_AT_LOCATION_RADIUS ||
            sp->EffectImplicitTargetA[1]==EFF_TARGET_INVISIBLE_OR_HIDDEN_ENEMIES_AT_LOCATION_RADIUS ||
            sp->EffectImplicitTargetA[2]==EFF_TARGET_INVISIBLE_OR_HIDDEN_ENEMIES_AT_LOCATION_RADIUS ||
            sp->EffectImplicitTargetB[0]==EFF_TARGET_INVISIBLE_OR_HIDDEN_ENEMIES_AT_LOCATION_RADIUS ||
            sp->EffectImplicitTargetB[1]==EFF_TARGET_INVISIBLE_OR_HIDDEN_ENEMIES_AT_LOCATION_RADIUS ||
            sp->EffectImplicitTargetB[2]==EFF_TARGET_INVISIBLE_OR_HIDDEN_ENEMIES_AT_LOCATION_RADIUS ||

            sp->EffectImplicitTargetA[0]==EFF_TARGET_ALL_ENEMIES_AROUND_CASTER ||
            sp->EffectImplicitTargetA[1]==EFF_TARGET_ALL_ENEMIES_AROUND_CASTER ||
            sp->EffectImplicitTargetA[2]==EFF_TARGET_ALL_ENEMIES_AROUND_CASTER ||
            sp->EffectImplicitTargetB[0]==EFF_TARGET_ALL_ENEMIES_AROUND_CASTER ||
            sp->EffectImplicitTargetB[1]==EFF_TARGET_ALL_ENEMIES_AROUND_CASTER ||
            sp->EffectImplicitTargetB[2]==EFF_TARGET_ALL_ENEMIES_AROUND_CASTER ||

            sp->EffectImplicitTargetA[0]==EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED ||
            sp->EffectImplicitTargetA[1]==EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED ||
            sp->EffectImplicitTargetA[2]==EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED ||
            sp->EffectImplicitTargetB[0]==EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED ||
            sp->EffectImplicitTargetB[1]==EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED ||
            sp->EffectImplicitTargetB[2]==EFF_TARGET_ALL_ENEMY_IN_AREA_CHANNELED ||

            sp->EffectImplicitTargetA[0]==EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
            sp->EffectImplicitTargetA[1]==EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
            sp->EffectImplicitTargetA[2]==EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
            sp->EffectImplicitTargetB[0]==EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
            sp->EffectImplicitTargetB[1]==EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
            sp->EffectImplicitTargetB[2]==EFF_TARGET_ALL_ENEMY_IN_AREA_INSTANT
        )
    {
        return true;
    }

    return false;
}