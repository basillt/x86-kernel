#ifndef NVME_H
#define NVME_H

#include "pci.h"

/* Read and print an NVMe controller's capability and version registers */
void nvme_probe(const struct pci_device *dev);

#endif
