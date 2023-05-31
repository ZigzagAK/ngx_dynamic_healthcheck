#!/bin/bash

# Copyright, Aleksey Konovkin (alkon2000@mail.ru)
# BSD license type

download=0
if [ "$1" == "1" ]; then
  download=1
fi
build_deps=0

DIR="$(pwd)"

VERSION="1.24.0"
PCRE2_VERSION="10.37"
ZLIB_VERSION="1.2.11"

SUFFIX=""

BASE_PREFIX="$DIR/build"
INSTALL_PREFIX="$DIR/install"

export PCRE_SOURCES="$DIR/build/pcre2-$PCRE2_VERSION"
export ZLIB_SOURCES="$DIR/build/zlib-$ZLIB_VERSION"

EMBEDDED_OPTS="--with-pcre=$PCRE_SOURCES --with-zlib=$ZLIB_SOURCES"

function clean() {
  rm -rf install  2>/dev/null
  rm -rf $(ls -1d build/* 2>/dev/null | grep -v deps)    2>/dev/null
  if [ $download -eq 1 ]; then
    rm -rf download 2>/dev/null
  fi
}

if [ "$1" == "clean" ]; then
  clean
  exit 0
fi

function build_debug() {
  cd nginx-$VERSION$SUFFIX
  echo "Configuring debug nginx-$VERSION$SUFFIX"
  ./configure --prefix="$INSTALL_PREFIX/nginx-$VERSION$SUFFIX" \
              $EMBEDDED_OPTS \
              --with-stream \
              --with-debug \
              --with-cc-opt="-O0" \
              --add-module=../../../ngx_dynamic_healthcheck \
              --add-module=../ngx_dynamic_upstream > /dev/null

  r=$?
  if [ $r -ne 0 ]; then
    exit $r
  fi

  echo "Build debug nginx-$VERSION$SUFFIX"
  make -j4 > /dev/null

  r=$?
  if [ $r -ne 0 ]; then
    exit $r
  fi
  make install > /dev/null

  mv "$INSTALL_PREFIX/nginx-$VERSION$SUFFIX/sbin/nginx" "$INSTALL_PREFIX/nginx-$VERSION$SUFFIX/sbin/nginx.debug"

  cd ..
}

function build_release() {
  cd nginx-$VERSION$SUFFIX
  echo "Configuring release nginx-$VERSION$SUFFIX"
  ./configure --prefix="$INSTALL_PREFIX/nginx-$VERSION$SUFFIX" \
              $EMBEDDED_OPTS \
              --with-stream \
              --add-module=../../../ngx_dynamic_healthcheck \
              --add-module=../ngx_dynamic_upstream > /dev/null

  r=$?
  if [ $r -ne 0 ]; then
    exit $r
  fi

  echo "Build release nginx-$VERSION$SUFFIX"
  make -j4 > /dev/null

  r=$?
  if [ $r -ne 0 ]; then
    exit $r
  fi
  make install > /dev/null
  cd ..
}

function download_module() {
  if [ -e $DIR/../$2 ]; then
    echo "Get $DIR/../$2"
    dir=$(pwd)
    cd $DIR/..
    tar zcf $dir/$2.tar.gz $(ls -1d $2/* | grep -vE "(install$)|(build$)|(download$)|(.git$)")
    cd $dir
  else
    if [ $download -eq 1 ] || [ ! -e $2.tar.gz ]; then
      echo "Download $2 branch=$3"
      curl -s -L -o $2.zip https://github.com/$1/$2/archive/$3.zip
      unzip -q $2.zip
      mv $2-* $2
      tar zcf $2.tar.gz $2
      rm -rf $2-* $2 $2.zip
    fi
  fi
}

function gitclone() {
  git clone $1 > /dev/null 2> /tmp/err
  if [ $? -ne 0 ]; then
    cat /tmp/err
  fi
}

function download_nginx() {
  if [ $download -eq 1 ] || [ ! -e nginx-$VERSION.tar.gz ]; then
    echo "Download nginx-$VERSION"
    curl -s -L -O http://nginx.org/download/nginx-$VERSION.tar.gz
  else
    echo "Get nginx-$VERSION.tar.gz"
  fi
}

function download_pcre() {
  if [ $download -eq 1 ] || [ ! -e pcre2-$PCRE2_VERSION.tar.gz ]; then
    echo "Download PCRE2-$PCRE2_VERSION"
    curl -s -L -O http://ftp.cs.stanford.edu/pub/exim/pcre/pcre2-$PCRE2_VERSION.tar.gz
  else
    echo "Get pcre2-$PCRE2_VERSION.tar.gz"
  fi
}

function download_dep() {
  if [ $download -eq 1 ] || [ ! -e $2-$3.tar.gz ]; then
    echo "Download $2-$3.$4"
    curl -s -L -o $2-$3.tar.gz $1/$2-$3.$4
  else
    echo "Get $2-$3.tar.gz"
  fi
}

function extract_downloads() {
  cd download

  for d in $(ls -1 *.tar.gz)
  do
    echo "Extracting $d"
    tar zxf $d -C ../build --keep-old-files 2>/dev/null
  done

  cd ..
}

function download() {
  mkdir build                2>/dev/null
  mkdir build/deps           2>/dev/null

  mkdir download             2>/dev/null
  mkdir download/lua_modules 2>/dev/null

  cd download

  download_pcre
  download_nginx

  download_module ZigzagAK    ngx_dynamic_upstream             master

  download_dep http://zlib.net                                 zlib      $ZLIB_VERSION      tar.gz

  cd ..
}

function install_file() {
  echo "Install $1"
  if [ ! -e "$INSTALL_PREFIX/nginx-$VERSION$SUFFIX/$2" ]; then
    mkdir -p "$INSTALL_PREFIX/nginx-$VERSION$SUFFIX/$2"
  fi
  cp -r $3 $1 "$INSTALL_PREFIX/nginx-$VERSION$SUFFIX/$2/"
}

function install_files() {
  for f in $(ls $1)
  do
    install_file $f $2 $3
  done
}

function build() {
  cd build

  make clean > /dev/null 2>&1
  build_debug

#  make clean > /dev/null 2>&1
#  build_release

  cd ..
}

clean
download
extract_downloads
build

install_file scripts/start.sh   .
install_file scripts/debug.sh   .
install_file scripts/restart.sh .
install_file scripts/stop.sh    .

cp LICENSE "$INSTALL_PREFIX/nginx-$VERSION$SUFFIX/LICENSE"

cd "$DIR"

kernel_name=$(uname -s)
kernel_version=$(uname -r)

cd install

tar zcvf nginx-$VERSION$SUFFIX-$kernel_name-$kernel_version.tar.gz nginx-$VERSION$SUFFIX
rm -rf nginx-$VERSION$SUFFIX

cd ..

exit $r