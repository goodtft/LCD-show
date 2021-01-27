#include "config.h"
#include "mailbox.h"
#include "util.h"
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <inttypes.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int vcio = -1;
void OpenMailbox()
{
  vcio = open("/dev/vcio", 0);
  if (vcio < 0) FATAL_ERROR("Failed to open VideoCore kernel mailbox!");
}

void CloseMailbox()
{
  close(vcio);
  vcio = -1;
}

// Sends a pointer to the given buffer over to the VideoCore mailbox. See https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
void SendMailbox(void *buffer)
{
  int ret = ioctl(vcio, _IOWR(/*MAJOR_NUM=*/100, 0, char *), buffer);
  if (ret < 0) FATAL_ERROR("SendMailbox failed in ioctl!");
}

// Defines the structure of a Mailbox message
template<int PayloadSize>
struct MailboxMessage
{
  MailboxMessage(uint32_t messageId):messageSize(sizeof(*this)), requestCode(0), messageId(messageId), messageSizeBytes(sizeof(uint32_t)*PayloadSize), dataSizeBytes(sizeof(uint32_t)*PayloadSize), messageEndSentinel(0) {}
  uint32_t messageSize;
  uint32_t requestCode;
  uint32_t messageId;
  uint32_t messageSizeBytes;
  uint32_t dataSizeBytes;
  union
  {
    uint32_t payload[PayloadSize];
    uint32_t result;
  };
  uint32_t messageEndSentinel;
};

// Sends a mailbox message with 1xuint32 payload
uint32_t Mailbox(uint32_t messageId, uint32_t payload0)
{
  MailboxMessage<1> msg(messageId);
  msg.payload[0] = payload0;
  SendMailbox(&msg);
  return msg.result;
}

uint32_t MailboxRet2(uint32_t messageId, uint32_t payload0)
{
  MailboxMessage<2> msg(messageId);
  msg.payload[0] = payload0;
  msg.payload[1] = 0;
  SendMailbox(&msg);
  return msg.payload[1];
}

// Sends a mailbox message with 3xuint32 payload
uint32_t Mailbox(uint32_t messageId, uint32_t payload0, uint32_t payload1, uint32_t payload2)
{
  MailboxMessage<3> msg(messageId);
  msg.payload[0] = payload0;
  msg.payload[1] = payload1;
  msg.payload[2] = payload2;
  SendMailbox(&msg);
  return msg.result;
}
