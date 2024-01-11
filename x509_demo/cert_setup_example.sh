#!/bin/bash
set -x

rm -f *.pem *.info

# Certificate Authority setup
# 1. create a private key for Certificate Authority(CA)
[ ! -f ca-priv-key.pem ] && certtool --generate-privkey > ca-priv-key.pem
cat ca-priv-key.pem

# 2. prepare a information configure file for self-signed Root Certificate
# note: cn means common name
if [ ! -f CA.info ]; then
    echo "cn = SmartX Inc" >> CA.info
    echo "ca" >> CA.info
    echo "cert_signing_key" >> CA.info
fi

# 3. create a self-signed Root Certificate for Certificate Authority using the CA private key
certtool --generate-self-signed \
         --load-privkey ca-priv-key.pem \
         --template CA.info \
         --outfile ca-cert.pem

# 4. peek the Root Certificate information
certtool -i --infile ca-cert.pem

# Issuing server certificate
# 1. create a private key for the server
certtool --generate-privkey > srv-priv-key.pem
cat srv-priv-key.pem

# 2. prepare a information configure file for server certificate
if [ ! -f Server.info ]; then
    echo "organization = SmartX Inc" >> Server.info
    echo "cn = server.elf.org" >> Server.info
    echo "ip_address = 192.168.11.11" >> Server.info
    echo "tls_www_server" >> Server.info
    echo "encryption_key" >> Server.info
    echo "signing_key" >> Server.info
fi

# 3. create server certificate signed by server private key while issued by CA
certtool --generate-certificate \
         --load-privkey srv-priv-key.pem \
         --load-ca-certificate ca-cert.pem \
         --load-ca-privkey ca-priv-key.pem \
         --template Server.info \
         --outfile srv-cert.pem

# 4. peek the server certificate information
certtool -i --infile srv-cert.pem

# Issuing client certificate
# 1. create a private key for the client
certtool --generate-privkey > cli-priv-key.pem
cat cli-priv-key.pem

# 2. prepare a information configure file for server certificate
if [ ! -f Client.info ]; then
    echo "country = America" >> Client.info
    echo "state = California" >> Client.info
    echo "locality = San Francisco" >> Client.info
    echo "organization = SmartX Inc" >> Client.info
    echo "cn = Google" >> Client.info
    echo "tls_www_client" >> Client.info
    echo "encryption_key" >> Client.info
    echo "signing_key" >> Client.info
fi

# 3. create client certificate signed by client private key while issued by CA
certtool --generate-certificate \
         --load-privkey cli-priv-key.pem \
         --load-ca-certificate ca-cert.pem \
         --load-ca-privkey ca-priv-key.pem \
         --template Client.info \
         --outfile cli-cert.pem

# 4. peek the client certificate information
certtool -i --infile cli-cert.pem

# Issuing level-2 server certificate
# 1. create a private key for the server
certtool --generate-privkey > srv-sub-priv-key.pem
cat srv-sub-priv-key.pem

# 2. prepare a information configure file for server certificate
if [ ! -f Server-sub.info ]; then
    echo "organization = SmartX Inc" >> Server-sub.info
    echo "cn = server.sub.elf.org" >> Server-sub.info
    echo "ip_address = 192.168.11.12" >> Server-sub.info
    echo "tls_www_server" >> Server-sub.info
    echo "encryption_key" >> Server-sub.info
    echo "signing_key" >> Server-sub.info
fi

# 3. create server certificate signed by server private key while issued by CA
certtool --generate-certificate \
         --load-privkey srv-sub-priv-key.pem \
         --load-ca-certificate srv-cert.pem \
         --load-ca-privkey srv-priv-key.pem \
         --template Server-sub.info \
         --outfile srv-sub-cert.pem

# 4. peek the server certificate information
certtool -i --infile srv-sub-cert.pem

# Issuing level-2 client certificate
# 1. create a private key for the client
certtool --generate-privkey > cli-sub-priv-key.pem
cat cli-sub-priv-key.pem

# 2. prepare a information configure file for server certificate
if [ ! -f Client-sub.info ]; then
    echo "country = America" >> Client-sub.info
    echo "state = California" >> Client-sub.info
    echo "locality = San Francisco" >> Client-sub.info
    echo "organization = SmartX Inc" >> Client-sub.info
    echo "cn = Google Virtualization Dep" >> Client-sub.info
    echo "tls_www_client" >> Client-sub.info
    echo "encryption_key" >> Client-sub.info
    echo "signing_key" >> Client-sub.info
fi

# 3. create client certificate signed by client private key while issued by CA
certtool --generate-certificate \
         --load-privkey cli-sub-priv-key.pem \
         --load-ca-certificate cli-cert.pem \
         --load-ca-privkey cli-priv-key.pem \
         --template Client-sub.info \
         --outfile cli-sub-cert.pem

# 4. peek the client certificate information
certtool -i --infile cli-sub-cert.pem
