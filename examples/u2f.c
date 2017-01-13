/**
 * Create a virtual mouse.
 * Compile me with: gcc mouse.c -o virtual_mouse -framework IOKit
 */

#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <sys/types.h>
#include <sys/kern_control.h>
#include <sys/ioctl.h>

unsigned char report_descriptor[] = {
    0x06, 0xd0, 0xf1, //USAGE_PAGE(FIDO Alliance)
    0x09, 0x01,       //USAGE(U2F HID Authenticator Device)
    0xa1, 0x01,       //COLLECTION(Application)
    0x09, 0x20,       //   USAGE(Input Report Data)
    0x15, 0x00,       //   LOGICAL_MINIMUM(0)
    0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM(255)
    0x75, 0x08,       //   REPORT_SIZE(8)
    0x95, 0x40,       //   REPORT_COUNT(64)
    0x81, 0x02,       //   INPUT(Data, Var, Abs)
    0x09, 0x21,       //   USAGE(Output Report Data)
    0x15, 0x00,       //   LOGICAL_MINIMUM(0)
    0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM(255)
    0x75, 0x08,       //   REPORT_SIZE(8)
    0x95, 0x40,       //   REPORT_COUNT(64)
    0x91, 0x02,       //   OUTPUT(Data, Var, Abs)
    0xc0              // END_COLLECTION
};

struct mouse_report_t {
    uint8_t buttons;
    int8_t x;
    int8_t y;
};

#define SERVICE_NAME "it_unbit_foohid"

#define FOOHID_CREATE 0  // create selector
#define FOOHID_SEND 2  // send selector

#define DEVICE_CTL_NAME "it.unbit.foohid.u2f"
#define DEVICE_NAME "Foohid Virtual U2F"
#define DEVICE_SN "SN 123456"

int main() {
    io_iterator_t iterator;
    io_service_t service;
    io_connect_t connection;

    // Get a reference to the IOService
    kern_return_t ret = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(SERVICE_NAME), &iterator);

    if (ret != KERN_SUCCESS) {
        printf("Unable to access IOService.\n");
        exit(1);
    }

    // Iterate till success
    int found = 0;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        ret = IOServiceOpen(service, mach_task_self(), 0, &connection);

        if (ret == KERN_SUCCESS) {
            found = 1;
            break;
        }

        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    if (!found) {
        printf("Unable to open IOService.\n");
        exit(1);
    }

    // Fill up the input arguments.
    uint32_t input_count = 10;
    uint64_t input[input_count];
    input[0] = (uint64_t) strdup(DEVICE_NAME);  // device name
    input[1] = strlen((char *)input[0]);  // name length
    input[2] = (uint64_t) report_descriptor;  // report descriptor
    input[3] = sizeof(report_descriptor);  // report descriptor len
    input[4] = (uint64_t) strdup(DEVICE_SN);  // serial number
    input[5] = strlen((char *)input[4]);  // serial number len
    input[6] = (uint64_t) 2;  // vendor ID
    input[7] = (uint64_t) 3;  // device ID
    input[8] = (uint64_t) strdup(DEVICE_CTL_NAME);  // ctl name for shared socket
    input[9] = strlen((char *)input[8]);  // ctl name len

    ret = IOConnectCallScalarMethod(connection, FOOHID_CREATE, input, input_count, NULL, 0);
    if (ret != KERN_SUCCESS) {
        printf("Unable to create HID device. May be fine if created previously.\n");
    }

    int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd == -1) {
        perror("Unable to create socket");
        exit(1);
    }

    struct ctl_info info;
    memset(&info, 0, sizeof(info));
    strncpy(info.ctl_name, DEVICE_CTL_NAME, sizeof(info.ctl_name));
    if (ioctl(fd, CTLIOCGINFO, &info)) {
        perror("Could not get ID for kernel control.\n");
        exit(-1);
    }

    struct sockaddr_ctl addr;
    bzero(&addr, sizeof(addr));
    addr.sc_len = sizeof(addr);
    addr.sc_family = AF_SYSTEM;
    addr.ss_sysaddr = AF_SYS_CONTROL;
    addr.sc_id = info.ctl_id;
    addr.sc_unit = 0;

    int r = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (r != 0) {
        fprintf(stderr, "connect failed: %d\n", r);
        exit(1);
    }



    // Arguments to be passed through the HID message.
    // struct mouse_report_t mouse;
    // uint32_t send_count = 4;
    // uint64_t send[send_count];
    // send[0] = (uint64_t)input[0];  // device name
    // send[1] = strlen((char *)input[0]);  // name length
    // send[2] = (uint64_t) &mouse;  // mouse struct
    // send[3] = sizeof(struct mouse_report_t);  // mouse struct len

    for(;;) {
        // mouse.buttons = 0;
        // mouse.x = rand();
        // mouse.y = rand();

        // ret = IOConnectCallScalarMethod(connection, FOOHID_SEND, send, send_count, NULL, 0);
        // if (ret != KERN_SUCCESS) {
        //     printf("Unable to send message to HID device.\n");
        // }

        unsigned char buf[256];
        ssize_t d = read(fd, buf, sizeof(buf));
        printf("read() returned %d\n", d);
        for (int i = 0; i < d; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n");
    }
}
