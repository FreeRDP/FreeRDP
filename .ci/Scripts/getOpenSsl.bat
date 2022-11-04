call %~dp0\getvars.bat

pushd .
cd %freeRdpDir%\..

rem checkout openssl
mkdir OpenSSL
cd OpenSSL
git clone https://github.com/openssl/openssl .
git branch buildOpenSSl %openSSLTag%
git checkout buildOpenSSl
popd