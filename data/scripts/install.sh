#!/bin/sh
# 1st arg is dir where unpack, $2 file to be installed
update_file() 
{

	FILE_IMAGE=$1
	FILE_NEW=$2
	MD5NEW=`md5sum $FILE_NEW | cut -d " " -f1`
	if [ -e $FILE_IMAGE ]; then
		MD5IMG=`md5sum $FILE_IMAGE | cut -d " " -f1`
	else
		MD5IMG=0
	fi

	if [ "$MD5NEW" = "$MD5IMG" ]; then
		echo "New and old File $FILE_IMAGE identical"
	else
		DIR=$(dirname "$FILE_IMAGE")
		if [ ! -d "${DIR}" ]; then
			mkdir -p ${DIR}
		fi
		echo "update $FILE_IMAGE"
		rm -f $FILE_IMAGE
		cp -d $FILE_NEW $FILE_IMAGE
	fi
}

cd $1
tar zxvf $2
./temp_inst/ctrl/preinstall.sh $PWD

PAT=$1/temp_inst/inst
len_a=${#PAT}

find $PAT  | while read i ;do
	if [ -d "$i" ] || [ $(basename "$i") == "boot.bmp.gz" ] || [ $(basename "$i") == "uImage" ];then 
		continue
	fi

	NEW_F=$i
	len_b=${#NEW_F}
	OLD_F=$(echo ${NEW_F:$len_a:$len_b})
	update_file $OLD_F $NEW_F
done

./temp_inst/ctrl/postinstall.sh $PWD
rm -rf temp_inst
exit 0
