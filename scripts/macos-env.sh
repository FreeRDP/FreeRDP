export PATH="/usr/local/opt/krb5/bin:$PATH"
export PATH="/usr/local/opt/krb5/sbin:$PATH"
export LDFLAGS="-L/usr/local/opt/krb5/lib -L/usr/local/lib"
export CPPFLAGS="-I/usr/local/opt/krb5/include"
export CFLAGS="-I/usr/local/include"

# set pkgconfig path to brew openssl
export PKG_CONFIG_PATH=/usr/local/opt/openssl/lib/pkgconfig
