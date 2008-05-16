# Isolate a list of nVidia GeForce G8X family device ids
# input = src/nv_driver.c (xserver-xorg-video-nv)
$0 == "/*************** G8x ***************/" {
	while(getline > 0) {
		# they are listed in one block
		if($2 !~ /^0x[0-9A-Z]+,$/)
			break
		# print device id and description
		print substr(tolower($2), 7, 4), $3, $4, $5
	}
}

$0 == "NVIsSupported(CARD32 id)" {
	while(getline > 0) {
		if($1 == "return")
			break
		if($2 ~ /^0x[0-9A-Z]+:$/)
			print substr(tolower($2), 3, 4) " \"Too new/Unknown\""
	}
}
