/***
 * Demonstrike Core
 */

#include "StdAfx.h"

#define MOVEMENT_PACKET_TIME_DELAY 500

void WorldSession::HandleMovementOpcodes( WorldPacket & recv_data )
{
    if(!_player->IsInWorld())
        return;
    else if(_player->GetPlayerStatus() == TRANSFER_PENDING)
        return;
    else if(_player->GetCharmedByGUID() || _player->GetTaxiState())
        return;

    // spell cancel on movement, for now only fishing is added
    if (GameObject* t_go = _player->m_SummonedObject)
    {
        if (t_go->GetEntry() == GO_FISHING_BOBBER)
            castPtr<GameObject>(t_go)->EndFishing(GetPlayer(),true);
    }

    uint32 mstime = getMSTime();
    if(sEventMgr.HasEvent(_player, EVENT_PLAYER_FORCE_LOGOUT))
    {
        if(_player->HasFlag(PLAYER_FLAGS, PLAYER_FLAG_AFK))
            sEventMgr.ModifyEventTimeAndTimeLeft(_player, EVENT_PLAYER_FORCE_LOGOUT, 1800000);
        else sEventMgr.RemoveEvents(_player, EVENT_PLAYER_FORCE_LOGOUT);
    }

    MovementInterface *moveInterface = _player->GetMovementInterface();
    if(!moveInterface->ReadFromClient(recv_data.GetOpcode(), &recv_data))
        Disconnect();
}

void WorldSession::HandleAcknowledgementOpcodes( WorldPacket & recv_data )
{
    if(!_player->IsInWorld())
        return;
    if(_player->GetPlayerStatus() == TRANSFER_PENDING)
        return;

    MovementInterface *moveInterface = _player->GetMovementInterface();
    if(!moveInterface->ReadFromClient(recv_data.GetOpcode(), &recv_data))
        Disconnect();
}

void WorldSession::HandleForceSpeedChangeOpcodes( WorldPacket & recv_data )
{
    if(!_player->IsInWorld())
        return;
    if(_player->GetPlayerStatus() == TRANSFER_PENDING)
        return;

    MovementInterface *moveInterface = _player->GetMovementInterface();
    if(!moveInterface->ReadFromClient(recv_data.GetOpcode(), &recv_data))
        Disconnect();
}

void WorldSession::HandleSetActiveMoverOpcode( WorldPacket & recv_data )
{
    _player->GetMovementInterface()->SetActiveMover(&recv_data);
}

void WorldSession::HandleMoveTimeSkippedOpcode( WorldPacket & recv_data )
{
    _player->GetMovementInterface()->MoveTimeSkipped(&recv_data);
}

void WorldSession::HandleMoveSplineCompleteOpcode(WorldPacket &recv_data)
{
    _player->GetMovementInterface()->MoveSplineComplete(&recv_data);
}

void WorldSession::HandleMoveFallResetOpcode(WorldPacket & recv_data)
{
    _player->GetMovementInterface()->MoveFallReset(&recv_data);
}

#define DO_BIT(read, buffer, val, result) if(read) val = (buffer.ReadBit() ? result : !result); else buffer.WriteBit(val ? result : !result);
#define DO_COND_BIT(read, buffer, cond, val) if(cond) { if(read) val = buffer.ReadBit(); else buffer.WriteBit(val); }
#define DO_BYTES(read, buffer, type, val) if(read) val = buffer.read<type>(); else buffer.append<type>(val);
#define DO_COND_BYTES(read, buffer, cond, type, val) if(cond) { if(read) val = buffer.read<type>(); else buffer.append<type>(val); }
#define DO_SEQ_BYTE(read, buffer, val) if(read) buffer.ReadByteSeq(val); else buffer.WriteByteSeq(val);

// Close with a semicolon
#define BUILD_BOOL_LIST()  bool hasMovementFlags = read ? false : m_movementFlagMask & 0x0F, \
    hasMovementFlags2 = read ? false : m_movementFlagMask & 0xF0, \
    hasTimestamp = read ? false : true, \
    hasOrientation = read ? false : !G3D::fuzzyEq(m_serverLocation->o, 0.0f), \
    hasTransportData = read ? false : !m_transportGuid.empty(), \
    hasSpline = read ? false : isSplineMovingActive(), \
    hasTransportTime2 = read ? false : (hasTransportData && m_transportTime2 != 0), \
    hasTransportVehicleId = read ? false : (hasTransportData && m_vehicleId != 0), \
    hasPitch = read ? false : (hasFlag(MOVEMENTFLAG_SWIMMING) || hasFlag(MOVEMENTFLAG_FLYING) || hasFlag(MOVEMENTFLAG_ALWAYS_ALLOW_PITCHING)), \
    hasFallDirection = read ? false : hasFlag(MOVEMENTFLAG_TOGGLE_FALLING), \
    hasFallData = read ? false : (hasFallDirection || m_jumpTime != 0), \
    hasSplineElevation = read ? false : hasFlag(MOVEMENTFLAG_SPLINE_ELEVATION)

void MovementInterface::HandlePlayerMove(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasTransportData, true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
}

void MovementInterface::HandleHeartbeat(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasSpline, true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleJump(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleFallLand(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleStartForward(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasFallData, true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleStartBackward(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasPitch, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleStartStrafeLeft(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleStartStrafeRight(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasSpline, true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleStartTurnLeft(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasFallData, true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleStartTurnRight(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleStartPitchDown(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasPitch, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleStartPitchUp(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleStartAscend(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleStartDescend(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleStartSwim(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleStop(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
}

void MovementInterface::HandleStopStrafe(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleStopTurn(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasFallData, true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleStopPitch(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasOrientation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
}

void MovementInterface::HandleStopAscend(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasSpline, true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleStopSwim(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
}

void MovementInterface::HandleSetFacing(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
}

void MovementInterface::HandleSetPitch(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleFallReset(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
}

void MovementInterface::HandleSetRunMode(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasTransportData, true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleSetWalkMode(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleSetPitchRate(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DoExtraData(MOVEMENT_CODE_SET_PITCH_RATE, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetCanFly(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleUnsetCanFly(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetHover(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleUnsetHover(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleWaterWalk(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleLandWalk(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleFeatherFall(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleNormalFall(bool read, ByteBuffer &buffer)
{
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleRoot(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleUnroot(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleUpdateKnockBack(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
}

void MovementInterface::HandleUpdateTeleport(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_BIT(read, buffer, hasSplineElevation, false);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
}

void MovementInterface::HandleChangeTransport(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasPitch, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleNotActiveMover(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasMovementFlags, false);

    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);

    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);

    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);

    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);

    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);

    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
}

void MovementInterface::HandleSetCollisionHeight(bool read, ByteBuffer &buffer)
{
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DoExtraData(MOVEMENT_CODE_SET_COLLISION_HEIGHT, read, &buffer);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleUpdateCollisionHeight(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DoExtraData(MOVEMENT_CODE_UPDATE_COLLISION_HEIGHT, read, &buffer);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
}

void MovementInterface::HandleUpdateWalkSpeed(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BIT(read, buffer, hasOrientation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DoExtraData(MOVEMENT_CODE_UPDATE_WALK_SPEED, read, &buffer);
}

void MovementInterface::HandleUpdateRunSpeed(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DoExtraData(MOVEMENT_CODE_UPDATE_RUN_SPEED, read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
}

void MovementInterface::HandleUpdateRunBackSpeed(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasOrientation, false);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DoExtraData(MOVEMENT_CODE_UPDATE_RUN_BACK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
}

void MovementInterface::HandleUpdateSwimSpeed(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DoExtraData(MOVEMENT_CODE_UPDATE_SWIM_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
}

void MovementInterface::HandleUpdateFlightSpeed(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DoExtraData(MOVEMENT_CODE_UPDATE_FLIGHT_SPEED, read, &buffer);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
}

void MovementInterface::HandleSetWalkSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DoExtraData(MOVEMENT_CODE_SET_WALK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetRunSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DoExtraData(MOVEMENT_CODE_SET_RUN_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetRunBackSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DoExtraData(MOVEMENT_CODE_SET_RUN_BACK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
}

void MovementInterface::HandleSetSwimSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DoExtraData(MOVEMENT_CODE_SET_SWIM_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetSwimBackSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DoExtraData(MOVEMENT_CODE_SET_SWIM_BACK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetFlightSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DoExtraData(MOVEMENT_CODE_SET_FLIGHT_SPEED, read, &buffer);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetFlightBackSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DoExtraData(MOVEMENT_CODE_SET_FLIGHT_BACK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSetTurnRate(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DoExtraData(MOVEMENT_CODE_SET_TURN_RATE, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckRoot(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckUnroot(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckFeatherFall(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckForceWalkSpeedChange(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DoExtraData(MOVEMENT_CODE_ACK_FORCE_WALK_SPEED_CHANGE, read, &buffer);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasSpline, true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckForceRunSpeedChange(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DoExtraData(MOVEMENT_CODE_ACK_FORCE_RUN_SPEED_CHANGE, read, &buffer);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckForceRunBackSpeedChange(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DoExtraData(MOVEMENT_CODE_ACK_FORCE_RUN_BACK_SPEED_CHANGE, read, &buffer);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckForceSwimSpeedChange(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DoExtraData(MOVEMENT_CODE_ACK_FORCE_SWIM_SPEED_CHANGE, read, &buffer);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckForceFlightSpeedChange(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DoExtraData(MOVEMENT_CODE_ACK_FORCE_FLIGHT_SPEED_CHANGE, read, &buffer);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckGravityEnable(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasOrientation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckGravityDisable(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckHover(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckWaterWalk(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckKnockBack(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasFallData, true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckSetCanFly(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckSetCollisionHeight(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DoExtraData(MOVEMENT_CODE_ACK_SET_COLLISION_HEIGHT, read, &buffer);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleAckSetCanTransitionBetweenSwimAndFly(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, uint32, read ? m_clientCounter : m_serverCounter);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasSpline, true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    m_incrementMoveCounter = true;
}

void MovementInterface::HandleSplineDone(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
}

void MovementInterface::HandleSplineSetWalkSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_WALK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
}

void MovementInterface::HandleSplineSetRunSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_RUN_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
}

void MovementInterface::HandleSplineSetRunBackSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_RUN_BACK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
}

void MovementInterface::HandleSplineSetSwimSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_SWIM_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
}

void MovementInterface::HandleSplineSetSwimBackSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_SWIM_BACK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
}

void MovementInterface::HandleSplineSetFlightSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_FLIGHT_SPEED, read, &buffer);
}

void MovementInterface::HandleSplineSetFlightBackSpeed(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_FLIGHT_BACK_SPEED, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
}

void MovementInterface::HandleSplineSetPitchRate(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_PITCH_RATE, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
}

void MovementInterface::HandleSplineSetTurnRate(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DoExtraData(MOVEMENT_CODE_SPLINE_SET_TURN_RATE, read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
}

void MovementInterface::HandleSplineSetWalkMode(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
}

void MovementInterface::HandleSplineSetRunMode(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
}

void MovementInterface::HandleSplineGravityEnable(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
}

void MovementInterface::HandleSplineGravityDisable(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
}

void MovementInterface::HandleSplineSetHover(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
}

void MovementInterface::HandleSplineSetUnhover(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
}

void MovementInterface::HandleSplineStartSwim(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
}

void MovementInterface::HandleSplineStopSwim(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
}

void MovementInterface::HandleSplineSetFlying(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
}

void MovementInterface::HandleSplineUnsetFlying(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
}

void MovementInterface::HandleSplineSetWaterWalk(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
}

void MovementInterface::HandleSplineSetLandWalk(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
}

void MovementInterface::HandleSplineSetFeatherFall(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
}

void MovementInterface::HandleSplineSetNormalFall(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
}

void MovementInterface::HandleSplineRoot(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
}

void MovementInterface::HandleSplineUnroot(bool read, ByteBuffer &buffer)
{
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
}

void MovementInterface::HandleDismissControlledVehicle(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasOrientation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
}

void MovementInterface::HandleChangeSeatsOnControlledVehicle(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 0);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 1);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 2);
    DO_BIT(read, buffer, hasOrientation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 3);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 4);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 5);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, hasPitch, false);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 6);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 7);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, hasSpline, true);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 8);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 9);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 10);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 11);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 12);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 13);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 14);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 15);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DoExtraData(MOVEMENT_CODE_CHANGE_SEATS_ON_CONTROLLED_VEHICLE, read, &buffer, 16);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
}

void MovementInterface::HandleEmbeddedMovement(bool read, ByteBuffer &buffer)
{
    BUILD_BOOL_LIST();
    DO_BYTES(read, buffer, float, read ? m_clientLocation.z : m_serverLocation->z);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.y : m_serverLocation->y);
    DO_BYTES(read, buffer, float, read ? m_clientLocation.x : m_serverLocation->x);
    DO_BIT(read, buffer, hasFallData, true);
    DO_BIT(read, buffer, hasTimestamp, false);
    DO_BIT(read, buffer, hasOrientation, false);
    read ? buffer.ReadBit() : buffer.WriteBit(0); // Skip_bit
    DO_BIT(read, buffer, hasSpline, true);
    DO_BIT(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4], true);
    DO_BIT(read, buffer, hasMovementFlags2, false);
    DO_BIT(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3], true);
    DO_BIT(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5], true);
    DO_BIT(read, buffer, hasSplineElevation, false);
    DO_BIT(read, buffer, hasPitch, false);
    DO_BIT(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7], true);
    DO_BIT(read, buffer, hasTransportData, true);
    DO_BIT(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2], true);
    DO_BIT(read, buffer, hasMovementFlags, false);
    DO_BIT(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportTime2);
    DO_BIT(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4], true);
    DO_COND_BIT(read, buffer, hasTransportData, hasTransportVehicleId);
    DO_BIT(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1], true);
    DO_BIT(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3], true);
    if(hasMovementFlags2) HandleMovementFlags2(read, &buffer);
    if(hasMovementFlags) HandleMovementFlags(read, &buffer);
    DO_COND_BIT(read, buffer, hasFallData, hasFallDirection);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[1] : m_moverGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[4] : m_moverGuid[4]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[7] : m_moverGuid[7]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[3] : m_moverGuid[3]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[0] : m_moverGuid[0]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[2] : m_moverGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[5] : m_moverGuid[5]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientGuid[6] : m_moverGuid[6]);
    DO_COND_BYTES(read, buffer, hasTransportData, int8, m_transportSeatId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.o : m_transportLocation.o);
    DO_COND_BYTES(read, buffer, hasTransportData, uint32, m_transportTime);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[6] : m_transportGuid[6]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[5] : m_transportGuid[5]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportVehicleId, uint32, m_vehicleId);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.x : m_transportLocation.x);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[4] : m_transportGuid[4]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.z : m_transportLocation.z);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[2] : m_transportGuid[2]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[0] : m_transportGuid[0]);
    DO_COND_BYTES(read, buffer, hasTransportData && hasTransportTime2, uint32, m_transportTime2);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[1] : m_transportGuid[1]);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[3] : m_transportGuid[3]);
    DO_COND_BYTES(read, buffer, hasTransportData, float, read ? m_clientTransLocation.y : m_transportLocation.y);
    DO_SEQ_BYTE(read, buffer, read ? m_clientTransGuid[7] : m_transportGuid[7]);
    DO_COND_BYTES(read, buffer, hasOrientation, float, read ? m_clientLocation.o : m_serverLocation->o);
    DO_COND_BYTES(read, buffer, hasSplineElevation, float, splineElevation);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpTime);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_XYSpeed);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_sin);
    DO_COND_BYTES(read, buffer, hasFallDirection, float, m_jump_cos);
    DO_COND_BYTES(read, buffer, hasFallData, float, m_jumpZSpeed);
    DO_COND_BYTES(read, buffer, hasTimestamp, uint32, read ? m_clientTime : m_serverTime);
    DO_COND_BYTES(read, buffer, hasPitch, float, pitching);
}

#undef BUILD_BOOL_LIST
#undef DO_BIT
#undef DO_BYTES
#undef DO_COND_BYTES
#undef DO_SEQ_BYTE
