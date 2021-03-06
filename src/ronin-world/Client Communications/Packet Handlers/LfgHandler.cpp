/*
 * Sandshroud Project Ronin
 * Copyright (C) 2005-2008 Ascent Team <http://www.ascentemu.com/>
 * Copyright (C) 2008-2009 AspireDev <http://www.aspiredev.org/>
 * Copyright (C) 2009-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StdAfx.h"

void WorldSession::HandleLFDPlrLockOpcode( WorldPacket& recv_data )
{
    CHECK_INWORLD_RETURN();

    DungeonSet::iterator itr;
    DungeonSet randomDungeonSet;
    LFGDungeonsEntry* dungeonEntry = NULL;
    uint32 level = _player->getLevel();
    DungeonSet LevelDungeonSet = sLfgMgr.GetLevelSet(level);
    for (itr = LevelDungeonSet.begin(); itr != LevelDungeonSet.end(); itr++)
    {
        dungeonEntry = dbcLFGDungeons.LookupEntry(*itr);
        if (dungeonEntry != NULL && dungeonEntry->LFGType == LFG_RANDOM
            && dungeonEntry->reqExpansion <= GetHighestExpansion()
            && dungeonEntry->minLevel <= level && level <= dungeonEntry->maxLevel)
            randomDungeonSet.insert(dungeonEntry->Id);
        dungeonEntry = NULL;
    }

    // Crow: Confirmed structure below
    WorldPacket data(SMSG_LFG_PLAYER_INFO, 400);
    uint8 randomsize = (uint8)randomDungeonSet.size();
    data << randomsize;
    for(itr = randomDungeonSet.begin(); itr != randomDungeonSet.end(); itr++)
    {
        dungeonEntry = dbcLFGDungeons.LookupEntry(*itr);
        data << uint32(dungeonEntry->GetUniqueID());

        uint8 done = 0;
        Quest* QuestReward = NULL;
        LfgReward* reward = sLfgMgr.GetLFGReward(*itr);
        if(reward)
        {
            QuestReward = sQuestMgr.GetQuestPointer(reward->reward[0].QuestId);
            if(QuestReward)
            {
                done = _player->HasFinishedQuest(reward->reward[0].QuestId);
                if(!done)
                    done = _player->HasFinishedDailyQuest(reward->reward[0].QuestId);
                if (done)
                    QuestReward = sQuestMgr.GetQuestPointer(reward->reward[1].QuestId);
            }
        }

        if (QuestReward)
        {
            data << uint8(done);
            data << uint32(sQuestMgr.GenerateRewardMoney(_player, QuestReward));
            data << uint32(sQuestMgr.GenerateQuestXP(_player, QuestReward)*sWorld.getRate(RATE_QUESTXP));
            data << uint32(reward->reward[done].MoneyReward);
            data << uint32(reward->reward[done].XPReward);
            data << uint8(QuestReward->count_reward_item);
            if (QuestReward->count_reward_item)
            {
                ItemPrototype* proto = NULL;
                for (uint8 i = 0; i < 4; i++)
                {
                    if (!QuestReward->reward_item[i])
                        continue;

                    proto = sItemMgr.LookupEntry(QuestReward->reward_item[i]);
                    data << uint32(QuestReward->reward_item[i]);
                    data << uint32(proto ? proto->DisplayInfoID : 0);
                    data << uint32(QuestReward->reward_itemcount[i]);
                }
            }
        }
        else
        {
            data << uint8(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint8(0);
        }
    }
    SendPacket(&data);
}

void WorldSession::HandleLFDPartyLockOpcode( WorldPacket& recv_data )
{
    WorldPacket data(SMSG_LFG_PARTY_INFO, 400);
    uint8 cnt = 0;
    data << uint8(cnt);
    for(uint8 i = 0; i < cnt; i++)
    {
        data << uint64(0);
        uint32 count = 0;
        data << count;
        for(uint32 i = 0; i < count; i++)
        {
            data << uint32(0);
            data << uint32(0);
        }
    }
    SendPacket(&data);
}
