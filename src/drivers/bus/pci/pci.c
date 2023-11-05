#define ODI_PCI_CONFIG_ADDR     0xCF8
#define ODI_PCI_CONFIG_DATA     0xCFC

#define ODI_PCI_CONFIGURATION_SPACE_SIZE 0x100

static void odi_pci_write32(uint32_t addr, uint32_t data) {
    addr &= ~(0b11);
    /* write address */
    odi_outb(ODI_PCI_CONFIG_ADDR, addr);
    /* write data */
    odi_outb(ODI_PCI_CONFIG_DATA, data);
}

static uint32_t odi_pci_read32(uint32_t addr) {
    addr &= ~(0b11);
    /* write address */
    odi_outb(ODI_PCI_CONFIG_ADDR, addr);
    /* read data */
    return odi_inl(ODI_PCI_CONFIG_DATA);
}

static uint16_t odi_pci_read16(uint32_t addr) {
    uint8_t offset = addr & 0xff;
    addr &= ~(0b11);
    return (uint16_t) ((odi_pci_read32(addr) >> ((offset & 0b10) * 0x10)) & 0xffff);
}

static uint8_t odi_pci_read8(uint32_t addr) {
    uint8_t offset = addr & 0xff;
    addr &= ~(0b11);
    return (uint16_t) ((odi_pci_read16(addr) >> ((offset & 0b1) * 0x8)) & 0xff);
}

static void odi_pci_memcpy8(void* dst, uint32_t src, size_t size){
    src &= ~(0b11);
    for(size_t i = 0; i < size; i += 0x1){
        *(uint8_t*)((uint64_t)dst + i) = odi_pci_read8(src + i);
    }
}

static void odi_pci_memcpy16(void* dst, uint32_t src, size_t size){
    src &= ~(0b11);
    for(size_t i = 0; i < size; i += 0x2){
        *(uint16_t*)((uint64_t)dst + i) = odi_pci_read16(src + i);
    }
}

static void odi_pci_memcpy32(void* dst, uint32_t src, size_t size){
    src &= ~(0b11);
    for(size_t i = 0; i < size; i += 0x4){
        *(uint32_t*)((uint64_t)dst + i) = odi_pci_read32(src + i);
    }
}

static uint32_t odi_pci_device_base_address(uint16_t bus, uint16_t device, uint16_t func){
    return (uint32_t) ((1 <<  31) | (bus << 16) | (device << 11) | (func << 8));
}

static bool odi_check_device_addr(uint32_t addr){
    uint16_t vendor_id = odi_pci_read16(addr + ODI_PCI_VENDOR_ID_OFFSET);
    if(vendor_id == 0xffff) return false;
    return true;
}

static bool odi_check_device(uint16_t bus, uint16_t device, uint16_t func){
    uint32_t addr = odi_pci_device_base_address(bus, device, func);
    return odi_check_device_addr(addr);
}

void odi_receive_configuration_space_pci(odi_pci_device_t* device){
    odi_pci_memcpy32(device->configuration_space, device->address, ODI_PCI_CONFIGURATION_SPACE_SIZE);
}

void odi_send_configuration_space_pci(odi_pci_device_t* device){
    uint64_t back_buffer = (uint64_t)device->configuration_space_back;
    uint64_t front_buffer = (uint64_t)device->configuration_space;
    uint64_t pci_address = device->address;

    // align buffer
    back_buffer &= ~(0b11);
    front_buffer &= ~(0b11);

    for(size_t i = 0; i < ODI_PCI_CONFIGURATION_SPACE_SIZE; i += 4){ // increment 32 bits
        if(*(uint32_t*)back_buffer != *(uint32_t*)front_buffer){
            odi_pci_write32(pci_address, *(uint32_t*)front_buffer);
        }
        pci_address += 4;
        back_buffer += 4;
        front_buffer += 4;
    }

    // update back buffer
    odi_dep_memcpy((void*)back_buffer, (void*)front_buffer, ODI_PCI_CONFIGURATION_SPACE_SIZE);
}

static void odi_get_device(odi_pci_device_list_info_t* pci_device_list, uint16_t bus, uint16_t device, uint16_t func){
    uint32_t address = odi_pci_device_base_address(bus, device, func);
    if(!odi_check_device_addr(address)) return; 

    odi_pci_device_t* device_info = (odi_pci_device_t*)odi_dep_malloc(sizeof(odi_pci_device_t));
    device_info->is_pcie = false;
    device_info->address = address; // this is not pci device, it's pcie device
    device_info->configuration_space = odi_dep_malloc(ODI_PCI_CONFIGURATION_SPACE_SIZE);
    device_info->configuration_space_back = odi_dep_malloc(ODI_PCI_CONFIGURATION_SPACE_SIZE);
    device_info->receive_configuration_space = &odi_receive_configuration_space_pci;
    device_info->send_configuration_space = &odi_send_configuration_space_pci;

    odi_pci_memcpy32(device_info->configuration_space_back, device_info->address, ODI_PCI_CONFIGURATION_SPACE_SIZE);
    odi_dep_memcpy(device_info->configuration_space, device_info->configuration_space_back, ODI_PCI_CONFIGURATION_SPACE_SIZE);

    odi_add_pci_device(pci_device_list, device_info);

    return;
}

void odi_pci_init(odi_pci_device_list_info_t* pci_device_list){
    for(uint32_t bus = 0; bus < 256; bus++) {
        for(uint32_t device = 0; device < 32; device++) {

            uint32_t addr = odi_pci_device_base_address(bus, device, 0);
            uint16_t vendor_id = odi_pci_read16(addr + ODI_PCI_VENDOR_ID_OFFSET);
            
            if(vendor_id == 0xffff) continue;

            uint8_t header_type = odi_pci_read8(addr + ODI_PCI_HEADER_TYPE_OFFSET);

            if((header_type & 0x80) != 0){
                for(uint32_t func = 0; func < 8; func++) {
                    odi_get_device(pci_device_list, bus, device, func);
                }
            }else{
                odi_get_device(pci_device_list, bus, device, 0);
            }
        }
    }    
}
