call %~dp0\getvars.bat

pushd .
cd %freeRdpDir%\..

rem checkout openssl
mkdir OpenSSL
cd OpenSSL
git clone --no-checkout --filter=tree:0 --depth=1 --single-branch --branch=%openSSLTag% https://github.com/openssl/openssl .
git branch buildOpenSSl %openSSLTag%
git checkout buildOpenSSl
popd