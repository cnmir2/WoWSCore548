/*
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2014 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
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

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "GuildMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Guild.h"
#include "GossipDef.h"
#include "SocialMgr.h"

void WorldSession::HandleGuildQueryOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guildGuid;
    ObjectGuid playerGuid;

    recvPacket.ReadGuidMask(playerGuid, 7, 3, 4);
    recvPacket.ReadGuidMask(guildGuid, 3, 4);
    recvPacket.ReadGuidMask(playerGuid, 2, 6);
    recvPacket.ReadGuidMask(guildGuid, 2, 5);
    recvPacket.ReadGuidMask(playerGuid, 1, 5);
    guildGuid[7] = recvPacket.ReadBit();
    playerGuid[0] = recvPacket.ReadBit();
    recvPacket.ReadGuidMask(guildGuid, 1, 6, 0);

    recvPacket.ReadByteSeq(playerGuid[7]);
    recvPacket.ReadGuidBytes(guildGuid, 2, 4, 7);
    recvPacket.ReadGuidBytes(playerGuid, 6, 0);
    recvPacket.ReadGuidBytes(guildGuid, 6, 0, 3);
    recvPacket.ReadByteSeq(playerGuid[2]);
    recvPacket.ReadByteSeq(guildGuid[5]);
    recvPacket.ReadByteSeq(playerGuid[3]);
    recvPacket.ReadByteSeq(guildGuid[1]);
    recvPacket.ReadGuidBytes(playerGuid, 4, 1, 5);

    TC_LOG_ERROR("guild", "CMSG_GUILD_QUERY [%s]: Guild: %u Target: %u",
        GetPlayerInfo().c_str(), GUID_LOPART(guildGuid), GUID_LOPART(playerGuid));

    if (Guild* guild = sGuildMgr->GetGuildByGuid(guildGuid))
        if (guild->IsMember(playerGuid))
            guild->HandleQuery(this);
}

void WorldSession::HandleGuildInviteOpcode(WorldPacket& recvPacket)
{
    uint32 nameLength = recvPacket.ReadBits(9);
    std::string invitedName = recvPacket.ReadString(nameLength);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_INVITE [%s]: Invited: %s", GetPlayerInfo().c_str(), invitedName.c_str());
    if (normalizePlayerName(invitedName))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleInviteMember(this, invitedName);
}

void WorldSession::HandleGuildRemoveOpcode(WorldPacket& recvPacket)
{
    ObjectGuid playerGuid;

    recvPacket.ReadGuidMask(playerGuid, 7, 3, 4, 2, 5, 6, 1, 0);

    recvPacket.ReadGuidBytes(playerGuid, 0, 2, 5, 6, 7, 1, 4, 3);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_REMOVE [%s]: Target: %u", GetPlayerInfo().c_str(), GUID_LOPART(playerGuid));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRemoveMember(this, playerGuid);
}

void WorldSession::HandleGuildAcceptOpcode(WorldPacket& /*recvPacket*/)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_ACCEPT [%s]", GetPlayerInfo().c_str());

    if (!GetPlayer()->GetGuildId())
        if (Guild* guild = sGuildMgr->GetGuildById(GetPlayer()->GetGuildIdInvited()))
            guild->HandleAcceptMember(this);
}

void WorldSession::HandleGuildDeclineOpcode(WorldPacket& /*recvPacket*/)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_DECLINE [%s]", GetPlayerInfo().c_str());

    Player* player = GetPlayer();
    player->SetGuildIdInvited(0);
    player->SetInGuild(0);

    uint64 inviterGuid = player->GetLastGuildInviterGUID();
    if (Player* inviter = ObjectAccessor::FindPlayer(inviterGuid))
        inviter->SendDeclineGuildInvitation(player->GetName());
}

void WorldSession::HandleGuildRosterOpcode(WorldPacket& recvPacket)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_ROSTER [%s]", GetPlayerInfo().c_str());
    recvPacket.rfinish();

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRoster(this);
    else
        Guild::SendCommandResult(this, GUILD_COMMAND_ROSTER, ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

void WorldSession::HandleGuildPromoteOpcode(WorldPacket& recvPacket)
{
    ObjectGuid targetGuid;

    recvPacket.ReadGuidMask(targetGuid, 6, 0, 4, 3, 1, 7, 2, 5);

    recvPacket.ReadGuidBytes(targetGuid, 1, 7, 2, 5, 3, 4, 0, 6);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_PROMOTE [%s]: Target: %u", GetPlayerInfo().c_str(), GUID_LOPART(targetGuid));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleUpdateMemberRank(this, targetGuid, false);
}

void WorldSession::HandleGuildDemoteOpcode(WorldPacket& recvPacket)
{
    ObjectGuid targetGuid;

    recvPacket.ReadGuidMask(targetGuid, 3, 6, 0, 2, 7, 5, 4, 1);

    recvPacket.ReadGuidBytes(targetGuid, 7, 4, 2, 5, 1, 3, 0, 6);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_DEMOTE [%s]: Target: %u", GetPlayerInfo().c_str(), GUID_LOPART(targetGuid));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleUpdateMemberRank(this, targetGuid, true);
}

void WorldSession::HandleGuildAssignRankOpcode(WorldPacket& recvPacket)
{
    ObjectGuid targetGuid;

    uint32 rankId;
    recvPacket >> rankId;

    recvPacket.ReadGuidMask(targetGuid, 2, 3, 1, 6, 0, 4, 7, 5);

    recvPacket.ReadGuidBytes(targetGuid, 7, 3, 2, 5, 6, 0, 4, 1);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_ASSIGN_MEMBER_RANK [%s]: Target: %u Rank: %u, Issuer: %u",
        GetPlayerInfo().c_str(), GUID_LOPART(targetGuid), rankId, GUID_LOPART(_player->GetGUID()));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMemberRank(this, targetGuid, _player->GetGUID(), rankId);
}

void WorldSession::HandleGuildLeaveOpcode(WorldPacket& /*recvPacket*/)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_LEAVE [%s]", GetPlayerInfo().c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleLeaveMember(this);
}

void WorldSession::HandleGuildDisbandOpcode(WorldPacket& /*recvPacket*/)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_DISBAND [%s]", GetPlayerInfo().c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleDisband(this);
}

void WorldSession::HandleGuildMOTDOpcode(WorldPacket& recvPacket)
{
    uint32 motdLength = recvPacket.ReadBits(10);
    std::string motd = recvPacket.ReadString(motdLength);
    TC_LOG_DEBUG("guild", "CMSG_GUILD_MOTD [%s]: MOTD: %s", GetPlayerInfo().c_str(), motd.c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
    {
        guild->HandleSetMOTD(this, motd);

        recvPacket.SetOpcode(SMSG_GUILD_MOTD);
        guild->BroadcastPacket(&recvPacket);
    }
}

void WorldSession::HandleGuildSetNoteOpcode(WorldPacket& recvPacket)
{
    ObjectGuid playerGuid;

    playerGuid[1] = recvPacket.ReadBit();
    uint32 length = recvPacket.ReadBits(8);
    recvPacket.ReadGuidMask(playerGuid, 4, 2);
    bool isPublic = recvPacket.ReadBit();
    recvPacket.ReadGuidMask(playerGuid, 3, 5, 0, 6, 7);
    
    recvPacket.ReadGuidBytes(playerGuid, 5, 1, 6);
    std::string note = recvPacket.ReadString(length);
    recvPacket.ReadGuidBytes(playerGuid, 0, 7, 4, 3, 2);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_NOTE [%s]: Target: %u, Note: %s, Public: %u",
        GetPlayerInfo().c_str(), GUID_LOPART(playerGuid), note.c_str(), isPublic);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMemberNote(this, note, playerGuid, isPublic);
}

void WorldSession::HandleGuildQueryRanksOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guildGuid;

    recvPacket.ReadGuidMask(guildGuid, 0, 2, 5, 4, 3, 7, 6, 1);

    recvPacket.ReadGuidBytes(guildGuid, 6, 0, 1, 7, 3, 2, 5, 4);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_QUERY_RANKS [%s]: Guild: %u",
        GetPlayerInfo().c_str(), GUID_LOPART(guildGuid));

    if (Guild* guild = sGuildMgr->GetGuildByGuid(guildGuid))
        if (guild->IsMember(_player->GetGUID()))
            guild->SendGuildRankInfo(this);
}

void WorldSession::HandleGuildAddRankOpcode(WorldPacket& recvPacket)
{
    uint32 rankId;
    recvPacket >> rankId;

    uint32 length = recvPacket.ReadBits(7);
    std::string rankName = recvPacket.ReadString(length);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_ADD_RANK [%s]: Rank: %s", GetPlayerInfo().c_str(), rankName.c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleAddNewRank(this, rankName);
}

void WorldSession::HandleGuildUpdateRanksOpcode(WorldPacket& recvPacket)
{
    uint32 GuildID = 0;
    bool RankUpdate = false;

    recvPacket >> GuildID;
    RankUpdate = recvPacket.ReadBit();

    TC_LOG_DEBUG("guild", "CMSG_GUILD_EVENT_UPDATE_RANKS for Guild id %u Rank: %u", GuildID, RankUpdate);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleUpdateRank(this, GuildID, RankUpdate);
}

void WorldSession::HandleGuildDelRankOpcode(WorldPacket& recvPacket)
{
    uint32 rankId;
    recvPacket >> rankId;

    TC_LOG_DEBUG("guild", "CMSG_GUILD_DEL_RANK [%s]: Rank: %u", GetPlayerInfo().c_str(), rankId);

    if (Guild* guild = GetPlayer()->GetGuild())
    {
        guild->HandleRemoveRank(this, rankId);
        guild->SendGuildRankInfo(this);
        guild->HandleQuery(this);
        guild->HandleRoster(this);
    }
}

void WorldSession::HandleGuildSwitchRankOpcode(WorldPacket& recvPacket)
{
    uint32 rankId;
    bool up;

    recvPacket >> rankId;
    up = recvPacket.ReadBit();

    TC_LOG_DEBUG("guild", "CMSG_GUILD_SWITCH_RANK [%s]: rank %u up %u", GetPlayerInfo().c_str(), rankId, up);

    if (Guild* guild = GetPlayer()->GetGuild())
    {
        if (GetPlayer()->GetGUID() != guild->GetLeaderGUID())
        {
            Guild::SendCommandResult(this, GUILD_COMMAND_INVITE, ERR_GUILD_PERMISSIONS);
            return;
        }

        guild->HandleSwitchRank(uint8(rankId), up);
        guild->SendGuildRankInfo(this);
        guild->HandleQuery(this);
        guild->HandleRoster(this);
    }
    else
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE, ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
}

void WorldSession::HandleGuildChangeInfoTextOpcode(WorldPacket& recvPacket)
{
    uint32 length = recvPacket.ReadBits(11);
    std::string info = recvPacket.ReadString(length);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_INFO_TEXT [%s]: %s", GetPlayerInfo().c_str(), info.c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetInfo(this, info);
}

void WorldSession::HandleSaveGuildEmblemOpcode(WorldPacket& recvPacket)
{
    uint64 vendorGuid;
    recvPacket >> vendorGuid;

    EmblemInfo emblemInfo;
    emblemInfo.ReadPacket(recvPacket);

    TC_LOG_DEBUG("guild", "MSG_SAVE_GUILD_EMBLEM [%s]: Guid: [" UI64FMTD
        "] Style: %d, Color: %d, BorderStyle: %d, BorderColor: %d, BackgroundColor: %d"
        , GetPlayerInfo().c_str(), vendorGuid, emblemInfo.GetStyle()
        , emblemInfo.GetColor(), emblemInfo.GetBorderStyle()
        , emblemInfo.GetBorderColor(), emblemInfo.GetBackgroundColor());

    if (GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_TABARDDESIGNER))
    {
        // Remove fake death
        if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
            GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleSetEmblem(this, emblemInfo);
        else
            Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_NOGUILD); // "You are not part of a guild!";
    }
    else
        Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_INVALIDVENDOR); // "That's not an emblem vendor!"
}

void WorldSession::HandleGuildEventLogQueryOpcode(WorldPacket& /* recvPacket */)
{
    TC_LOG_DEBUG("guild", "MSG_GUILD_EVENT_LOG_QUERY [%s]", GetPlayerInfo().c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendEventLog(this);
}

void WorldSession::HandleGuildBankMoneyWithdrawn(WorldPacket& /* recvPacket */)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_MONEY_WITHDRAWN [%s]", GetPlayerInfo().c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendMoneyInfo(this);
}

void WorldSession::HandleGuildPermissions(WorldPacket& /* recvPacket */)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_PERMISSIONS [%s]", GetPlayerInfo().c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendPermissions(this);
}

// Called when clicking on Guild bank gameobject
void WorldSession::HandleGuildBankerActivate(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    bool sendAllSlots;

    guid[3] = recvPacket.ReadBit();
    sendAllSlots = recvPacket.ReadBit();
    recvPacket.ReadGuidMask(guid, 0, 7, 1, 5, 2, 6, 4);

    recvPacket.ReadGuidBytes(guid, 7, 1, 0, 6, 4, 2, 5, 3);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANKER_ACTIVATE [%s]: Go: [" UI64FMTD "] AllSlots: %u"
        , GetPlayerInfo().c_str(), (uint64)guid, sendAllSlots);

    GameObject const* const go = GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_GUILD_BANK);
    if (!go)
        return;

    Guild* const guild = GetPlayer()->GetGuild();
    if (!guild)
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_VIEW_TAB, ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    guild->SendBankList(this, 0, true, true);
}

// Called when opening guild bank tab only (first one)
void WorldSession::HandleGuildBankQueryTab(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint8 tabId;
    bool sendAllSlots;

    recvPacket >> tabId;

    recvPacket.ReadGuidMask(guid, 7, 3);
    sendAllSlots = recvPacket.ReadBit();
    recvPacket.ReadGuidMask(guid, 0, 2, 4, 1, 6, 5);

    recvPacket.ReadGuidBytes(guid, 3, 7, 6, 4, 2, 5, 0, 1);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_QUERY_TAB [%s]: Go: [" UI64FMTD "], TabId: %u, AllSlots: %u"
        , GetPlayerInfo().c_str(), (uint64)guid, tabId, sendAllSlots);

    if (GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->SendBankList(this, tabId, true, false);
}

void WorldSession::HandleGuildBankDepositMoney(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint64 money;

    recvPacket >> money;
    recvPacket.ReadGuidMask(guid, 2, 7, 6, 4, 0, 1, 5, 3);

    recvPacket.ReadGuidBytes(guid, 1, 4, 5, 0, 2, 7, 6, 3);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_DEPOSIT_MONEY [%s]: Go: [" UI64FMTD "], money: " UI64FMTD,
        GetPlayerInfo().c_str(), (uint64)guid, money);

    if (GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_GUILD_BANK))
        if (money && GetPlayer()->HasEnoughMoney(money))
            if (Guild* guild = GetPlayer()->GetGuild())
                guild->HandleMemberDepositMoney(this, money);
}

void WorldSession::HandleGuildBankWithdrawMoney(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint64 money;
    recvPacket >> money;

    recvPacket.ReadGuidMask(guid, 1, 3, 7, 6, 5, 0, 4, 2);

    recvPacket.ReadGuidBytes(guid, 0, 7, 4, 2, 1, 6, 3, 5);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_WITHDRAW_MONEY [%s]: Go: [" UI64FMTD "], money: " UI64FMTD,
        GetPlayerInfo().c_str(), (uint64)guid, money);

    if (money && GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleMemberWithdrawMoney(this, money);
}

void WorldSession::HandleGuildBankSwapItems(WorldPacket& recvPacket)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_SWAP_ITEMS [%s]", GetPlayerInfo().c_str());

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
    {
        recvPacket.rfinish();                   // Prevent additional spam at rejected packet
        return;
    }

    ObjectGuid banker;
    uint32 stackCount, itemId, itemId2 = 0, bankItemCount = 0;
    uint8 bankSlot, bankTab, toSlot, containerSlot = NULL_BAG, containerItemSlot = NULL_SLOT, bankSlot2 = 0, bankTab2 = 0;
    bool hasContainerSlot, hasContainerItemSlot, hasItemId2, hasBankSlot2, hasBankTab2, hasBankItemCount, bankOnly, autoStore;

    recvPacket >> stackCount;
    recvPacket >> bankSlot;
    recvPacket >> toSlot;
    recvPacket >> itemId;
    recvPacket >> bankTab;

    banker[5] = recvPacket.ReadBit();
    hasBankTab2 = !recvPacket.ReadBit();
    banker[1] = recvPacket.ReadBit();
    hasContainerSlot = !recvPacket.ReadBit();
    autoStore = recvPacket.ReadBit();
    banker[0] = recvPacket.ReadBit();
    hasItemId2 = !recvPacket.ReadBit();
    hasBankSlot2 = !recvPacket.ReadBit();
    banker[2] = recvPacket.ReadBit();
    bankOnly = recvPacket.ReadBit();
    recvPacket.ReadGuidMask(banker, 4, 7, 3);
    hasContainerItemSlot = !recvPacket.ReadBit();
    banker[6] = recvPacket.ReadBit();
    hasBankItemCount = !recvPacket.ReadBit();

    recvPacket.ReadGuidBytes(banker, 2, 6, 5, 4, 0, 3, 1, 7);

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(banker, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        recvPacket.rfinish();                   // Prevent additional spam at rejected packet
        return;
    }

    if (hasItemId2)
        recvPacket >> itemId2;                  // source item id

    if (hasContainerSlot)
        recvPacket >> containerSlot;

    if (hasContainerItemSlot)
        recvPacket >> containerItemSlot;

    if (hasBankSlot2)
        recvPacket >> bankSlot2;                // source bank slot

    if (hasBankItemCount)
        recvPacket >> bankItemCount;

    if (hasBankTab2)
        recvPacket >> bankTab2;                 // source bank tab

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(banker, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        recvPacket.rfinish();                   // Prevent additional spam at rejected packet
        return;
    }

    if (bankOnly)
        guild->SwapItems(GetPlayer(), bankTab2, bankSlot2, bankTab, bankSlot, stackCount);
    else
    {
        if (!Player::IsInventoryPos(containerSlot, containerItemSlot) && !(containerSlot == NULL_BAG && containerItemSlot == NULL_SLOT))
            GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, NULL);
        else
            guild->SwapItemsWithInventory(GetPlayer(), toSlot != 0, bankTab, bankSlot, containerSlot, containerItemSlot, stackCount);
    }
}


void WorldSession::HandleGuildBankBuyTab(WorldPacket& recvPacket)
{
    uint8 tabId;
    ObjectGuid guid;
    recvPacket >> tabId;
    recvPacket.ReadGuidMask(guid, 0, 1, 3, 7, 2, 6, 5, 4);

    recvPacket.ReadGuidBytes(guid, 1, 4, 6, 7, 3, 5, 2, 0);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_BUY_TAB [%s]: Go: [" UI64FMTD "], TabId: %u", GetPlayerInfo().c_str(), (uint64)guid, tabId);

    // Since Cata you can buy tabs from the guild constrol tab - no need to check for guid
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleBuyBankTab(this, tabId);
}

void WorldSession::HandleGuildBankUpdateTab(WorldPacket& recvPacket)
{
    uint32 iconLen, nameLen;
    std::string name, icon;
    ObjectGuid guid;
    uint8 tabId;

    recvPacket >> tabId;
    guid[5] = recvPacket.ReadBit();
    iconLen = recvPacket.ReadBits(9);
    recvPacket.ReadGuidMask(guid, 1, 4, 2, 7, 0, 6, 3);
    nameLen = recvPacket.ReadBits(7);

    recvPacket.ReadGuidBytes(guid, 7, 4);
    icon = recvPacket.ReadString(iconLen);
    recvPacket.ReadGuidBytes(guid, 5, 1, 0);
    name = recvPacket.ReadString(nameLen);
    recvPacket.ReadGuidBytes(guid, 2, 3, 6);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_UPDATE_TAB [%s]: Go: [" UI64FMTD "], TabId: %u, Name: %s, Icon: %s"
        , GetPlayerInfo().c_str(), (uint64)guid, tabId, name.c_str(), icon.c_str());
    if (!name.empty() && !icon.empty())
        if (GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_GUILD_BANK))
            if (Guild* guild = GetPlayer()->GetGuild())
                guild->HandleSetBankTabInfo(this, tabId, name, icon);
}

void WorldSession::HandleGuildBankLogQuery(WorldPacket& recvPacket)
{
    uint32 tabId;
    recvPacket >> tabId;

    TC_LOG_DEBUG("guild", "MSG_GUILD_BANK_LOG_QUERY [%s]: TabId: %u", GetPlayerInfo().c_str(), tabId);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendBankLog(this, tabId);
}

void WorldSession::HandleQueryGuildBankTabText(WorldPacket &recvPacket)
{
    uint32 tabId;
    recvPacket >> tabId;

    TC_LOG_DEBUG("guild", "MSG_QUERY_GUILD_BANK_TEXT [%s]: TabId: %u", GetPlayerInfo().c_str(), tabId);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendBankTabText(this, tabId);
}

void WorldSession::HandleSetGuildBankTabText(WorldPacket& recvPacket)
{
    uint32 tabId;
    recvPacket >> tabId;

    uint32 textLen = recvPacket.ReadBits(14);
    std::string text = recvPacket.ReadString(textLen);

    TC_LOG_DEBUG("guild", "CMSG_SET_GUILD_BANK_TEXT [%s]: TabId: %u, Text: %s", GetPlayerInfo().c_str(), tabId, text.c_str());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SetBankTabText(tabId, text);
}

void WorldSession::HandleGuildQueryXPOpcode(WorldPacket& recvPacket)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_QUERY_GUILD_XP");

    ObjectGuid guildGuid;

    recvPacket.ReadGuidMask(guildGuid, 5, 6, 0, 1, 3, 7, 4, 2);

    recvPacket.ReadGuidBytes(guildGuid, 4, 6, 3, 0, 7, 5, 2, 1);

    TC_LOG_DEBUG("guild", "CMSG_QUERY_GUILD_XP [%s]: Guild: %u", GetPlayerInfo().c_str(), GUID_LOPART(guildGuid));

    if (Guild* guild = sGuildMgr->GetGuildByGuid(guildGuid))
        if (guild->IsMember(_player->GetGUID()))
            guild->SendGuildXP(this);
}

void WorldSession::HandleGuildSetRankPermissionsOpcode(WorldPacket& recvPacket)
{
    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
    {
        recvPacket.rfinish();
        return;
    }

    uint32 oldRankId;
    uint32 newRankId;
    uint32 oldRights;
    uint32 newRights;
    uint32 moneyPerDay;

    recvPacket >> oldRankId;

    GuildBankRightsAndSlotsVec rightsAndSlots(GUILD_BANK_MAX_TABS);
    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
    {
        uint32 bankRights;
        uint32 slots;

        recvPacket >> bankRights;
        recvPacket >> slots;

        rightsAndSlots[tabId] = GuildBankRightsAndSlots(tabId, uint8(bankRights), slots);
    }

    recvPacket >> moneyPerDay;
    recvPacket >> oldRights;
    recvPacket >> newRights;
    recvPacket >> newRankId;

    uint32 nameLength = recvPacket.ReadBits(7);
    std::string rankName = recvPacket.ReadString(nameLength);

    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_RANK_PERMISSIONS [%s]: Rank: %s (%u)", GetPlayerInfo().c_str(), rankName.c_str(), newRankId);

    guild->HandleSetRankInfo(this, newRankId, rankName, newRights, moneyPerDay, rightsAndSlots);
}

void WorldSession::HandleGuildRequestPartyState(WorldPacket& recvPacket)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_GUILD_REQUEST_PARTY_STATE");

    ObjectGuid guildGuid;

    recvPacket.ReadGuidMask(guildGuid, 0, 6, 7, 3, 5, 1, 2, 4);

    recvPacket.ReadGuidBytes(guildGuid, 6, 3, 2, 1, 5, 0, 7, 4);

    if (Guild* guild = sGuildMgr->GetGuildByGuid(guildGuid))
        guild->HandleGuildPartyRequest(this);
}

void WorldSession::HandleGuildRequestMaxDailyXP(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    recvPacket.ReadGuidMask(guid, 0, 3, 5, 1, 4, 6, 7, 2);

    recvPacket.ReadGuidBytes(guid, 7, 4, 3, 5, 1, 2, 6, 0);

    if (Guild* guild = sGuildMgr->GetGuildByGuid(guid))
    {
        if (guild->IsMember(_player->GetGUID()))
        {
            WorldPacket data(SMSG_GUILD_MAX_DAILY_XP, 8);
            data << uint64(sWorld->getIntConfig(CONFIG_GUILD_DAILY_XP_CAP));
            SendPacket(&data);
        }
    }
}

void WorldSession::HandleAutoDeclineGuildInvites(WorldPacket& recvPacket)
{
    uint8 enable = recvPacket.ReadBit();
    GetPlayer()->ApplyModFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_AUTO_DECLINE_GUILD, enable);
}

void WorldSession::HandleGuildRewardsQueryOpcode(WorldPacket& recvPacket)
{
    recvPacket.read_skip<uint32>(); // Unk

    if (sGuildMgr->GetGuildById(_player->GetGuildId()))
    {
        std::vector<GuildReward> const& rewards = sGuildMgr->GetGuildRewards();

        WorldPacket data(SMSG_GUILD_REWARDS_LIST, 3 + rewards.size() * (4 + 4 + 4 + 8 + 4 + 4));
        data.WriteBits(rewards.size(), 19);

        for (uint32 i = 0; i < rewards.size(); i++)
            data.WriteBits(rewards[i].Achievements.size(), 22);

        data.FlushBits();

        for (uint32 i = 0; i < rewards.size(); i++)
        {
            for (uint32 j = 0; j < rewards[i].Achievements.size(); j++)
                data << uint32(rewards[i].Achievements[j]);

            data << int32(rewards[i].Racemask);
            data << uint32(rewards[i].Entry);
            data << uint32(0); // minGuildLevel
            data << uint32(rewards[i].Standing);
            data << uint64(rewards[i].Price);
        }

        data << uint32(time(NULL));
        SendPacket(&data);
    }
}

void WorldSession::HandleGuildQueryNewsOpcode(WorldPacket& recvPacket)
{
    recvPacket.rfinish();

    TC_LOG_DEBUG("guild", "CMSG_GUILD_QUERY_NEWS [%s]", GetPlayerInfo().c_str());
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendNewsUpdate(this);
}

void WorldSession::HandleGuildNewsUpdateStickyOpcode(WorldPacket& recvPacket)
{
    uint32 newsId;
    bool sticky;
    ObjectGuid guid;

    recvPacket >> newsId;

    recvPacket.ReadGuidMask(guid, 6, 0);
    sticky = recvPacket.ReadBit();
    recvPacket.ReadGuidMask(guid, 2, 7, 5, 4, 3, 1);

    recvPacket.ReadGuidBytes(guid, 5, 4, 0, 1, 6, 2, 3, 7);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleNewsSetSticky(this, newsId, sticky);
}

void WorldSession::HandleGuildSetGuildMaster(WorldPacket& recvPacket)
{
    uint8 nameLength = recvPacket.ReadBits(9);
    std::string playerName = recvPacket.ReadString(nameLength);

    normalizePlayerName(playerName);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetNewGuildMaster(this, playerName);
}

void WorldSession::HandleGuildReplaceGuildMaster(WorldPacket& /*recvPacket*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleReplaceGuildMaster(this);
}

void WorldSession::HandleGuildRequestChallengeUpdate(WorldPacket& recvPacket)
{
    WorldPacket data(SMSG_GUILD_CHALLENGE_UPDATED, 4 * 6 * 5);
    for (int i = 0; i < 6; i++)
        data << uint32(GuildChallengeWeeklyMaximum[i]);

    for (int i = 0; i < 6; i++)
        data << uint32(GuildChallengeGoldReward[i]);

    for (int i = 0; i < 6; i++)
        data << uint32(GuildChallengeMaxLevelGoldReward[i]);

    for (int i = 0; i < 6; i++)
        data << uint32(GuildChallengeXPReward[i]);

    for (int i = 0; i < 6; i++)
        data << uint32(0); // Progress - NYI
    SendPacket(&data);
}

void WorldSession::HandleGuildBankTabNote(WorldPacket& recvData)
{
    uint32 tabId, noteLength;
    std::string note;

    recvData >> tabId;

    noteLength = recvData.ReadBits(14);
    recvData.FlushBits();

    note = recvData.ReadString(noteLength);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetBankTabNote(this, tabId, note);
}
