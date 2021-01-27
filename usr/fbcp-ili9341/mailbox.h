#pragma once

#include <inttypes.h>

void OpenMailbox(void);
void CloseMailbox(void);
void SendMailbox(void *buffer);
uint32_t Mailbox(uint32_t messageId, uint32_t payload0);
uint32_t MailboxRet2(uint32_t messageId, uint32_t payload0);
uint32_t Mailbox(uint32_t messageId, uint32_t payload0, uint32_t payload1, uint32_t payload2);
