#pragma once

typedef struct tagWebSocketHeader
{
	unsigned char major;
	unsigned char minor;
	unsigned short type;
	unsigned int length;
} WebSocketHeader;

typedef struct tagVideoDataHeader
{
	unsigned int eFrameType;
	unsigned int sequence;
	unsigned int option;
} VideoDataHeader;

enum eMessageType
{
	kMsgTypeConnect = 100,
	kMsgTypeHeartbeat = 101,

	kMsgTypeStartStream = 1001,
	kMsgTypeStartStreamAck = 1002,
	kMsgTypeVideoData = 1003,
	kMsgTypeVideoAck = 1004,
	kMsgTypeRequestKeyFrame = 1005,
	kMsgTypeAudioData = 1006,
	kMsgTypeStreamQuit = 1007,
	kMsgTypeStopStream = 1008,
	kMsgTypeStopStreamAck = 1009,

	kMsgTypeMessage = 2001,
	kMsgTypeMessageAck = 2002,
	kMsgTypeAssignController = 2003
};

