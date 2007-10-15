# input = src/atipciids.h (xserver-xorg-video-ati)
{
	if($3 !~ /^0x[0-9A-Z]+$/)
		next
	if($2 !~ /^PCI_CHIP_R[A-Z]?[2-4]/)
		next
	sub(/^PCI_CHIP_/, "", $2)
	print substr(tolower($3), 3, 4), $2
}
