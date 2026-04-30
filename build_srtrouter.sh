#!/bin/sh

curDir="$( cd "$( dirname "$0" )" &> /dev/null && pwd )"

cd ${curDir}/SrtRouter
chmod +x CreateMakefile.sh
./CreateMakefile.sh
cd build
make
make install

echo "build finish"
cd ${curDir}

echo "package...."
if [ -f ${curDir}/SrtRouter/build/bin/version.properties ]; then
	pkg_version="$(cat ${curDir}/SrtRouter/build/bin/version.properties)"
	echo "pkg_version: ${pkg_version}"
else
	pkg_version="Internal-1.0.0.0"
	echo ${pkg_version} > ${curDir}/SrtRouter/build/bin/version.properties
fi

srtrouterFile="srtrouter_${pkg_version}.tar.gz"
echo "srtrouterFile: ${srtrouterFile}"
cd ${curDir}/SrtRouter/build/bin
tar vzcf ${curDir}/${srtrouterFile} * 
cd ${curDir}

echo "zip files successful."
echo ${curDir}/${srtrouterFile}
exit 0
