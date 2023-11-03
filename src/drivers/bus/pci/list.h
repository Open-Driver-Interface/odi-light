#ifndef ODI_DRIVERS_BUS_PCI_LIST_H
#define ODI_DRIVERS_BUS_PCI_LIST_H

pci_device_list_info_t* pci_list_init(void);

void add_pci_device(pci_device_list_info_t* devices_list, pci_device_t* device);

void convert_list_to_array(pci_device_list_info_t* devices_list, pci_device_array_info_t* devices_array);

#endif