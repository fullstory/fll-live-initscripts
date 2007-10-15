# input file = src/nv_driver.c (xserver-xorg-video-nv)
$0 == "/*************** G8x ***************/" {
	while(getline > 0) {
		# they are listed in one block
		if($2 !~ /^0x[0-9A-Z]+,$/)
			break
		# print device id and description
		print substr(tolower($2), 7, 4), $3, $4, $5
	}
}
