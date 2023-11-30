package main

import (
	"fmt"
	"regexp"
)

func main() {
	// libvirt-0-format true
	// libvirt-0-virtio-format true
	// libvirt-0-scsi-format true
	// libvirt--format true
	cmdFormat := regexp.MustCompile(`libvirt-[[:alnum:]]+-*[[:alnum:]]*-format`)

	fmt.Printf("libvirt-0-format matched: %t\n", cmdFormat.MatchString("libvirt-0-format"))
	fmt.Printf("libvirt--format matched: %t\n", cmdFormat.MatchString("libvirt--format"))
	fmt.Printf("libvirt-0-virtio-format matched: %t\n", cmdFormat.MatchString("libvirt-0-virtio-format"))
	fmt.Printf("libvirt-0-scsi-format matched: %t\n", cmdFormat.MatchString("libvirt-0-scsi-format"))
}
