#!/system/bin/sh
echo "\n\nWelcome to cam update enter!!!\n\n" > /dev/kmsg
if [ -e /data/sme/update_log.txt ];then
    echo "have updated, exit\n"
else
    if [ -e /etc/cam/firmware*.bin ];then
        /system/bin/cam_update  /etc/cam/ /data/sme/ &
        if [ $? -eq 0 ];then
            touch /data/sme/update_log.txt
            echo "update success\n"
        else
            echo "update failed\n"
        fi
    else
        echo "not found update file\n"
    fi
fi
