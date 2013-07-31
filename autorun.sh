export LD_LIBRARY_PATH=/opt/lib:/opt/usr/lib
export PATH=${PATH}:/opt/bin:/opt/usr/bin

echo "### Starting NEUTRINO ###"

cd /tmp
/bin/neutrino > /dev/null 2> /dev/null

/bin/sync
/bin/sync

if [ -e /tmp/.reboot ] ; then
    /bin/dt -t"Rebooting..."
    /sbin/reboot -f
else
    /bin/dt -t"Panic..."
    sleep 5
    /sbin/reboot -f
fi
