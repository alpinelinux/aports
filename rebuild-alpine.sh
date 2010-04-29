rootdir=$(pwd)

distclean () {
    local allpkgs=$(find $rootdir -maxdepth 3 -name APKBUILD -print | sed -e 's/\/APKBUILD//g' | sort)
    for p in $allpkgs ; do
        cd $p
        abuild clean            2>&1
        abuild cleanoldpkg      2>&1
        abuild cleanpkg         2>&1
        abuild cleancache       2>&1
    done
}

build () { 
    local pkgs
    local maintainer
    pkgs=$(./aport.lua deplist $rootdir $1)
    for p in $pkgs ; do
        echo "Building $p"
        cd $rootdir/$1/$p
        abuild -rm > $rootdir/$1_$p.txt 2>&1 
        if [ "$?" = "0" ] ; then
            rm $rootdir/$1_$p.txt
        else
            maintainer=$(grep Maintainer APKBUILD | cut -d " " -f 3-)
            if [ -z "$maintainer" ] ; then
                maintainer="default maintainer"
            fi
            echo "Package $1/$p failed to build (output in $rootdir/$1_$p.txt)"
            echo "Package $1/$p failed to build. Notify $maintainer. Output is attached" | email -s "NOT SPAM $p build report" -a $rootdir/$1_$p.txt -n AlpineBuildBot -f build@alpinelinux.org amanison@anselsystems.com
        fi
    done
    cd $rootdir
}

echo "Removing traces of previous builds"
tmp=$(distclean)

echo "Refresh aports tree"
git pull

for s in main testing nonfree unstable ; do
    echo "Building packages in $s"
    build $s
done

echo "Done"

