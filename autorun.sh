export LD_LIBRARY_PATH=/opt/lib:/opt/usr/lib
export PATH=${PATH}:/opt/bin:/opt/usr/bin

echo "### Starting NEUTRINO ###"

cd /opt/bin
./neutrino &

