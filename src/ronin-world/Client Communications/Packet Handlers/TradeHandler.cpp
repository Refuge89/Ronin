/***
 * Demonstrike Core
 */

#include "StdAfx.h"
void WorldSession::SendTradeStatus(uint32 TradeStatus)
{
    OutPacket(SMSG_TRADE_STATUS, 4, &TradeStatus);
};

void WorldSession::HandleInitiateTrade(WorldPacket & recv_data)
{
    CHECK_INWORLD_RETURN();

    uint64 guid;
    recv_data >> guid;

    uint32 TradeStatus = TRADE_STATUS_PROPOSED;

    Player* pTarget = _player->GetMapMgr()->GetPlayer((uint32)guid);
    if(pTarget == NULL || !pTarget->IsInWorld())
        TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;
    else
    {
        // Handle possible error outcomes
        if(_player->isDead())
            TradeStatus = TRADE_STATUS_DEAD;
        else if(pTarget->isDead())
            TradeStatus = TRADE_STATUS_TARGET_DEAD;
        else if(_player->IsStunned())
            TradeStatus = TRADE_STATUS_YOU_STUNNED;
        else if(pTarget->IsStunned())
            TradeStatus = TRADE_STATUS_TARGET_STUNNED;
        else if(!pTarget->mTradeTarget.empty())
            TradeStatus = TRADE_STATUS_ALREADY_TRADING;
        else if(pTarget->GetTeam() != _player->GetTeam() && GetPermissionCount() == 0)
            TradeStatus = TRADE_STATUS_WRONG_FACTION;
        else if(pTarget->m_ignores.find(_player->GetGUID()) != pTarget->m_ignores.end())
            TradeStatus = TRADE_STATUS_PLAYER_IGNORED;
        else if(_player->CalcDistance(pTarget) > 10.0f)     // This needs to be checked
            TradeStatus = TRADE_STATUS_TOO_FAR_AWAY;
    }

    if(TradeStatus != TRADE_STATUS_PROPOSED)
    {
        _player->ResetTradeVariables();
        SendTradeStatus(TradeStatus);
    }
    else
    {
        _player->ResetTradeVariables();
        pTarget->ResetTradeVariables();

        pTarget->mTradeTarget = _player->GetLowGUID();
        _player->mTradeTarget = pTarget->GetLowGUID();

        pTarget->mTradeStatus = TradeStatus;
        _player->mTradeStatus = TradeStatus;

        WorldPacket data(SMSG_TRADE_STATUS, 12);
        data << TradeStatus;
        data << _player->GetGUID();

        if(pTarget->m_session && pTarget->m_session->GetSocket())
            pTarget->m_session->SendPacket(&data);
    }
}

void WorldSession::HandleBeginTrade(WorldPacket & recv_data)
{
    Player* pTarget = NULL;
    uint32 TradeStatus = TRADE_STATUS_INITIATED;

    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;
    else
    {
        pTarget = _player->GetTradeTarget();
        if(pTarget == NULL || !pTarget->IsInWorld() )
            TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;
        else if( _player->CalcDistance( pTarget ) > 10.0f )
            TradeStatus = TRADE_STATUS_TOO_FAR_AWAY;
    }

    //Abort if not ok
    if( TradeStatus != TRADE_STATUS_INITIATED)
    {
        TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;
        _player->ResetTradeVariables();
        SendTradeStatus(TradeStatus);
    }
    else
    {
        //Update status
        WorldPacket data(SMSG_TRADE_STATUS, 8);
        data << TradeStatus << uint32(0x19);

        SendPacket(&data);
        if(pTarget->m_session && pTarget->m_session->GetSocket())
            pTarget->m_session->SendPacket(&data);

        _player->mTradeStatus = TradeStatus;
        pTarget->mTradeStatus = TradeStatus;
    }
}

void WorldSession::HandleBusyTrade(WorldPacket & recv_data)
{
    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        return;

    uint32 TradeStatus = TRADE_STATUS_PLAYER_BUSY;

    Player* pTarget = _player->GetTradeTarget();
    if(pTarget == NULL || !pTarget->IsInWorld())
        TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;

    _player->ResetTradeVariables();

    SendTradeStatus(TradeStatus);
}

void WorldSession::HandleIgnoreTrade(WorldPacket & recv_data)
{
    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        return;

    uint32 TradeStatus = TRADE_STATUS_PLAYER_IGNORED;

    Player* pTarget = _player->GetTradeTarget();
    if(pTarget == NULL || !pTarget->IsInWorld())
        TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;

    _player->ResetTradeVariables();

    SendTradeStatus(TradeStatus);
}

void WorldSession::HandleCancelTrade(WorldPacket & recv_data)
{
    if(_player)
    {
        if(!_player->IsInWorld() || _player->mTradeTarget.empty())
            return;

        if( _player->mTradeStatus == TRADE_STATUS_COMPLETE)
        {
            _player->ResetTradeVariables();
            return;
        }

        uint32 TradeStatus = TRADE_STATUS_CANCELLED;

        Player* pTarget = _player->GetTradeTarget();
        if(pTarget == NULL || !pTarget->IsInWorld())
        {
            TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;
            _player->ResetTradeVariables();
            SendTradeStatus(TradeStatus);
        }
        else
        {
            pTarget->ResetTradeVariables();
            _player->ResetTradeVariables();

            SendTradeStatus(TradeStatus);
            if(pTarget->m_session && pTarget->m_session->GetSocket())
                pTarget->m_session->SendTradeStatus(TradeStatus);
        }
    }
}

void WorldSession::HandleUnacceptTrade(WorldPacket & recv_data)
{
    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        return;

    uint32 TradeStatus = TRADE_STATUS_UNACCEPTED;

    Player* pTarget = _player->GetTradeTarget();
    if(pTarget == NULL || !pTarget->IsInWorld())
    {
        TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;
        _player->ResetTradeVariables();
        SendTradeStatus(TradeStatus);
    }
    else
    {
        pTarget->mTradeStatus = TradeStatus;
        _player->mTradeStatus = TradeStatus;

        SendTradeStatus(TradeStatus);
        if( pTarget->m_session && pTarget->m_session->GetSocket())
            pTarget->m_session->SendTradeStatus(TradeStatus);
    }
}

void WorldSession::HandleSetTradeItem(WorldPacket & recv_data)
{
    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        return;

    uint8 TradeSlot = recv_data.contents()[0];
    int8 SourceBag = recv_data.contents()[1];
    uint8 SourceSlot = recv_data.contents()[2];

    Player* pTarget = _player->GetTradeTarget();
    if(pTarget == NULL || !pTarget->IsInWorld() || TradeSlot > 6)
        return;

    Item* pItem = _player->GetItemInterface()->GetInventoryItem(SourceBag, SourceSlot);
    if( pItem == NULL )
        return;

    if(pItem->IsContainer())
    {
        if( pItem->IsContainer() && castPtr<Container>(pItem)->HasItems() )
        {
            _player->GetItemInterface()->BuildInventoryChangeError( pItem, NULL, INV_ERR_CANT_TRADE_EQUIP_BAGS);
            return;
        }
    }

    if(pItem->IsAccountbound())
    {
        _player->GetItemInterface()->BuildInventoryChangeError( pItem, NULL, INV_ERR_ACCOUNT_BOUND);
        return;
    }

    for(uint32 i = 0; i < 7; i++)
    {
        // duping little shits
        if(_player->mTradeItems[i] == pItem || pTarget->mTradeItems[i] == pItem)
        {
            sWorld.LogCheater(this, "tried to dupe an item through trade");
            Disconnect();

            uint8 TradeStatus = TRADE_STATUS_CANCELLED;
            if( pTarget->m_session && pTarget->m_session->GetSocket())
                pTarget->m_session->SendTradeStatus(TradeStatus);
            return;
        }
    }

    _player->mTradeItems[TradeSlot] = pItem;
    _player->SendTradeUpdate();
}

void WorldSession::HandleSetTradeGold(WorldPacket & recv_data)
{
    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        return;

    uint32 Gold;
    recv_data >> Gold;

    if(_player->mTradeGold != Gold)
    {
        _player->mTradeGold = (Gold > _player->GetUInt32Value(PLAYER_FIELD_COINAGE) ? _player->GetUInt32Value(PLAYER_FIELD_COINAGE) : Gold);
        _player->SendTradeUpdate();
    }
}

void WorldSession::HandleClearTradeItem(WorldPacket & recv_data)
{
    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        return;

    uint8 TradeSlot = recv_data.contents()[0];
    if(TradeSlot > 6)
        return;

    _player->mTradeItems[TradeSlot] = NULL;
    _player->SendTradeUpdate();
}

void WorldSession::HandleAcceptTrade(WorldPacket & recv_data)
{
    if(!_player->IsInWorld() || _player->mTradeTarget.empty())
        return;

    uint32 TradeStatus = TRADE_STATUS_ACCEPTED;
    Player* pTarget = _player->GetTradeTarget();
    if(pTarget == NULL || !pTarget->IsInWorld())
        TradeStatus = TRADE_STATUS_PLAYER_NOT_FOUND;

    // Tell the other player we're green.
    if(pTarget->m_session && pTarget->m_session->GetSocket())
        pTarget->m_session->SendTradeStatus(TradeStatus);

    _player->mTradeStatus = TradeStatus;

    //Both sides accepted? Let's trade!
    if(_player->mTradeStatus == TRADE_STATUS_ACCEPTED && pTarget->mTradeStatus == TRADE_STATUS_ACCEPTED)
    {
        // Ready!
        uint32 ItemCount = 0;
        uint32 TargetItemCount = 0;
        Item* pItem;

        // Count items on both sides, check if bags are empty.
        for(uint32 Index = 0; Index < 6; Index++)
        {
            if(_player->mTradeItems[Index] != NULL)
            {
                pItem = _player->mTradeItems[Index];
                if( pItem != NULL && pItem->IsContainer() && castPtr<Container>(pItem)->HasItems())
                {
                    sWorld.LogCheater(this, "%s involved in bag-trick trade with %s", _player->GetName(),pTarget->GetName());
                    _player->GetItemInterface()->BuildInventoryChangeError( pItem, NULL, INV_ERR_CANT_TRADE_EQUIP_BAGS);
                    TradeStatus = TRADE_STATUS_CANCELLED;
                    break;
                }
                else
                    ++ItemCount;
            }

            if(pTarget->mTradeItems[Index] != NULL)
            {
                pItem = pTarget->mTradeItems[Index];
                if( pItem != NULL && pItem->IsContainer() && castPtr<Container>(pItem)->HasItems() )
                {
                    sWorld.LogCheater(this, "%s involved in bag-trick trade with %s.", pTarget->GetName(),_player->GetName());
                    pTarget->GetItemInterface()->BuildInventoryChangeError( pItem, NULL, INV_ERR_CANT_TRADE_EQUIP_BAGS);
                    TradeStatus = TRADE_STATUS_CANCELLED;
                    break;
                }
                else
                    ++TargetItemCount;
            }
        }

        //Do we have something to trade?
        if( ItemCount == 0 && TargetItemCount == 0 && _player->mTradeGold == 0 && pTarget->mTradeGold == 0 )
            TradeStatus = TRADE_STATUS_CANCELLED;
        //Do we have enough free slots on both sides?
        else if((_player->GetItemInterface()->CalculateFreeSlots(NULL) + ItemCount) < TargetItemCount || (pTarget->GetItemInterface()->CalculateFreeSlots(NULL) + TargetItemCount) < ItemCount )
            TradeStatus = TRADE_STATUS_CANCELLED;
        //Everything still ok?
        if(TradeStatus == TRADE_STATUS_ACCEPTED)
        {
            uint64 Guid;

            //Swapp 6 itemslots (7th will not trade)
            for(uint32 Index = 0; Index < 6; Index++)
            {
                Guid = _player->mTradeItems[Index] ? _player->mTradeItems[Index]->GetGUID() : 0;
                if(Guid != 0)
                {
                    if( _player->mTradeItems[Index]->GetBoundStatus() )
                        _player->GetItemInterface()->BuildInventoryChangeError( _player->mTradeItems[Index], NULL, INV_ERR_CANNOT_TRADE_THAT);
                    else
                    {
                        //Remove from player
                        pItem = _player->GetItemInterface()->SafeRemoveAndRetreiveItemByGuidRemoveStats(Guid, true);

                        //and add to pTarget
                        if(pItem != NULL)
                        {
                            pItem->SetOwner(pTarget);
                            sQuestMgr.OnPlayerDropItem(_player, pItem->GetEntry());
                            if( !pTarget->GetItemInterface()->AddItemToFreeSlot(pItem) )
                            {
                                pItem->Destruct();
                                pItem = NULL;
                            }
                        }

                        if(GetPermissionCount()>0 || pTarget->GetSession()->GetPermissionCount()>0)
                            sWorld.LogGM(this, "trade item %s with %s (soulbound = %d)", _player->mTradeItems[Index]->GetProto()->Name1, pTarget->GetName());
                    }
                }

                Guid = pTarget->mTradeItems[Index] ? pTarget->mTradeItems[Index]->GetGUID() : 0;
                if(Guid != 0)
                {
                    if( pTarget->mTradeItems[Index]->GetBoundStatus() )
                        pTarget->GetItemInterface()->BuildInventoryChangeError( pTarget->mTradeItems[Index], NULL, INV_ERR_CANNOT_TRADE_THAT);
                    else
                    {
                        //Remove from pTarget
                        pItem = pTarget->GetItemInterface()->SafeRemoveAndRetreiveItemByGuidRemoveStats(Guid, true);

                        //and add to initiator
                        if(pItem != NULL)
                        {
                            pItem->SetOwner(_player);
                            sQuestMgr.OnPlayerDropItem(pTarget, pItem->GetEntry());
                            if( !_player->GetItemInterface()->AddItemToFreeSlot(pItem) )
                            {
                                pItem->Destruct();
                                pItem = NULL;
                            }
                        }

                        if(GetPermissionCount()>0 || pTarget->GetSession()->GetPermissionCount()>0)
                            sWorld.LogGM(this, "trade item %s with %s", pTarget->mTradeItems[Index]->GetProto()->Name1, _player->GetName());
                    }
                }
            }

            // Trade Gold
            if(_player->mTradeGold)
            {
                pTarget->ModUnsigned32Value(PLAYER_FIELD_COINAGE, _player->mTradeGold);
                _player->ModUnsigned32Value(PLAYER_FIELD_COINAGE, -(int32)_player->mTradeGold);
            }
            if(pTarget->mTradeGold)
            {
                _player->ModUnsigned32Value(PLAYER_FIELD_COINAGE, pTarget->mTradeGold);
                pTarget->ModUnsigned32Value(PLAYER_FIELD_COINAGE, -(int32)pTarget->mTradeGold);
            }

            TradeStatus = TRADE_STATUS_COMPLETE;
        }

        SendTradeStatus(TradeStatus);
        pTarget->m_session->SendTradeStatus(TradeStatus);

        _player->mTradeStatus = TRADE_STATUS_COMPLETE;
        pTarget->mTradeStatus = TRADE_STATUS_COMPLETE;

        // Reset Trade Vars
        _player->ResetTradeVariables();
        pTarget->ResetTradeVariables();

        pTarget->SaveToDB(false);
        _player->SaveToDB(false);
    }
}