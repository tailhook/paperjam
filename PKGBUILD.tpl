# Maintainer: Paul Colomiets <paul@colomiets.name>

pkgname=paperjam
pkgver=${VERSION}
pkgrel=1
pkgdesc="A device implementation for zeromq and crossroads io"
arch=('i686' 'x86_64')
url="http://github.com/tailhook/paperjam"
license=('MIT')
# You can remove zeromq or libxs from dependencies
depends=('zeromq' 'libxs')
makedepends=('coyaml')
source=(https://github.com/downloads/tailhook/paperjam/$pkgname-$pkgver.tar.gz)
md5sums=('${DIST_MD5}')

build() {
  cd $srcdir/$pkgname-$pkgver
  ./waf configure --prefix=/usr
  ./waf build
}

check() {
  cd $srcdir/$pkgname-$pkgver
  ./waf test
}

package() {
  cd $srcdir/$pkgname-$pkgver
  ./waf install --destdir=$pkgdir
  install -D -m644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
