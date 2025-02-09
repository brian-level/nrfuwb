
#pragma once

// message id
//
// FROM controller TO accessory
//
#define UWBMSG_CONFIG_DATA      (0x01)
#define UWBMSG_DID_START        (0x02)
#define UWBMSG_DID_STOP         (0x03)

// FROM acessoroy TO controller
//
#define UWBMSG_INITIALIZE       (0x0A)
#define UWBMSG_CONFIG_AND_START (0x0B)
#define UWBMSG_STOP             (0x0C)

