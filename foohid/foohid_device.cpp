#include <IOKit/IOLib.h>
#include "foohid_device.h"
#include "debug.h"

#define super IOHIDDevice
OSDefineMetaClassAndStructors(it_unbit_foohid_device, IOHIDDevice)

bool it_unbit_foohid_device::init(OSDictionary *dict) {
    LogD("Initializing a new virtual HID device.");
    
    if (!super::init(dict)) {
        return false;
    }
    
    if (isMouse) {
        setProperty("HIDDefaultBehavior", "Mouse");
    } else if (isKeyboard) {
        setProperty("HIDDefaultBehavior", "Keyboard");
    }
    
    return true;
}

bool it_unbit_foohid_device::start(IOService *provider) {
    LogD("Executing 'it_unbit_foohid_device::start()'.");
    return super::start(provider);
}

void it_unbit_foohid_device::stop(IOService *provider) {
    LogD("Executing 'it_unbit_foohid_device::stop()'.");
    
    super::stop(provider);
}

void it_unbit_foohid_device::free() {
    LogD("Executing 'it_unbit_foohid_device::free()'.");
    
    if (reportDescriptor) IOFree(reportDescriptor, reportDescriptor_len);
    if (m_name) m_name->release();
    if (m_serial_number_string) m_serial_number_string->release();
    if (m_kern_ctl_ref) ctl_deregister(m_kern_ctl_ref);
    
    super::free();
}

OSString *it_unbit_foohid_device::name() {
    return m_name;
}

void it_unbit_foohid_device::setName(OSString *name) {
    if (name) name->retain();
    m_name = name;
}

void it_unbit_foohid_device::setSerialNumberString(OSString *serialNumberString) {
    if (serialNumberString) {
        serialNumberString->retain();
    }
    
    m_serial_number_string = serialNumberString;
}

void it_unbit_foohid_device::setVendorID(uint32_t vendorID) {
    m_vendor_id = OSNumber::withNumber(vendorID, 32);
}

void it_unbit_foohid_device::setProductID(uint32_t productID) {
    m_product_id = OSNumber::withNumber(productID, 32);
}

IOReturn it_unbit_foohid_device::newReportDescriptor(IOMemoryDescriptor **descriptor) const {
    LogD("Executing 'it_unbit_foohid_device::newReportDescriptor()'.");
    IOBufferMemoryDescriptor *buffer =
        IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, 0, reportDescriptor_len);
    
    if (!buffer) {
        LogD("Error while allocating new IOBufferMemoryDescriptor.");
        return kIOReturnNoResources;
    }
    
    buffer->writeBytes(0, reportDescriptor, reportDescriptor_len);
    *descriptor = buffer;
    
    return kIOReturnSuccess;
}

OSString *it_unbit_foohid_device::newProductString() const {
    m_name->retain();
    return m_name;
}

OSString *it_unbit_foohid_device::newSerialNumberString() const {
    m_serial_number_string->retain();
    return m_serial_number_string;
}

OSNumber *it_unbit_foohid_device::newVendorIDNumber() const {
    m_vendor_id->retain();
    return m_vendor_id;
}

OSNumber *it_unbit_foohid_device::newProductIDNumber() const {
    m_product_id->retain();
    return m_product_id;
}

IOReturn it_unbit_foohid_device::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options) {
    LogD("got report from host");

    if (m_ctlConnected) {
        IOMemoryMap *map = report->map();
        ctl_enqueuedata(m_kern_ctl_ref, m_ctlUnit, (void *)map->getAddress(), map->getLength(), 0);
        map->release();
    }
    return super::setReport(report, reportType, options);
}


static errno_t ctl_setopt_handler( kern_ctl_ref ctlref, unsigned int unit, void *userdata, int opt, void *data, size_t len )
{
    return KERN_SUCCESS;
}

static errno_t ctl_getopt_handler(kern_ctl_ref ctlref, unsigned int unit, void *userdata, int opt, void *data, size_t *len)
{
    return KERN_SUCCESS;
}

static errno_t ctl_disconnect_handler(kern_ctl_ref ctlref, unsigned int unit, void *unitinfo)
{
    it_unbit_foohid_device *t = (it_unbit_foohid_device *)unitinfo;

    t->ctlDisconnected();
    return KERN_SUCCESS;
}

static errno_t ctl_send_handler(kern_ctl_ref ctlref, unsigned int unit, void *userdata, mbuf_t m, int flags)
{
    return KERN_SUCCESS;
}

//XXX how do i get the instance in a way that's safe for multiple device objects?
//    do i need a lookup table? what happens if the object gets freed while the
//    connection is still open?
static it_unbit_foohid_device *global_dev;

static errno_t ctl_connect_handler(kern_ctl_ref ctlref, struct sockaddr_ctl *sac, void **unitinfo)
{
    *unitinfo = global_dev;
    it_unbit_foohid_device *t = (it_unbit_foohid_device *)*unitinfo;

    LogD("client connected with sc_unit %d", sac->sc_unit);
    t->ctlConnectedWithUnit(sac->sc_unit);
    return KERN_SUCCESS;
}

IOReturn it_unbit_foohid_device::createCtlSocket(OSString *ctlName) {
    struct kern_ctl_reg ctl_reg;
    errno_t err;

    bzero(&ctl_reg, sizeof(struct kern_ctl_reg));
    ctl_reg.ctl_id    = 0;
    ctl_reg.ctl_unit  = 0;
    if (ctlName->getLength() + 1 > MAX_KCTL_NAME) {
        LogD("ctl name specified is too long");
        return kIOReturnBadArgument;
    }
    strncpy(ctl_reg.ctl_name, ctlName->getCStringNoCopy(), MAX_KCTL_NAME);
    ctl_reg.ctl_flags         = 0; //CTL_FLAG_PRIVILEGED & CTL_FLAG_REG_ID_UNIT;
    ctl_reg.ctl_send          = ctl_send_handler;
    ctl_reg.ctl_getopt        = ctl_getopt_handler;
    ctl_reg.ctl_setopt        = ctl_setopt_handler;
    ctl_reg.ctl_connect       = ctl_connect_handler;
    ctl_reg.ctl_disconnect    = ctl_disconnect_handler;
    global_dev = this;

    err = ctl_register(&ctl_reg, &m_kern_ctl_ref);
    if (err == KERN_SUCCESS) {
        LogD("Register KerCtlConnection success: id=%d", ctl_reg.ctl_id);
    }
    else {
        LogD("Fail to register: err=%d",err);
    }

    return kIOReturnSuccess;
}

void it_unbit_foohid_device::ctlDisconnected() {
    m_ctlConnected = false;
    m_ctlUnit = 0;
}

void it_unbit_foohid_device::ctlConnectedWithUnit(unsigned int ctlUnit) {
    m_ctlConnected = true;
    m_ctlUnit = ctlUnit;
}
