#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

typedef struct tagMsgHeader
{
    unsigned char major;
    unsigned char minor;
    unsigned short type;
    unsigned int length;
} MsgHeader;

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

typedef enum
{
	videoFrameTypeInvalid, ///< encoder not ready or parameters are invalidate
	videoFrameTypeIDR,     ///< IDR frame in H.264
	videoFrameTypeI,       ///< I frame type
	videoFrameTypeP,       ///< P frame type
	videoFrameTypeSkip,    ///< skip the frame based encoder kernel
	videoFrameTypeIPMixed  ///< a frame where I and P slices are mixing, not supported yet
} EVideoFrameType;

#endif
