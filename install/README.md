# MUSL Cross Compiling tools and libraries installation guide

### download the cross tool package from:

https://more.musl.cc/x86_64-linux-musl/

Select the right package file for your platform. It should contain the name `arm`, `musleabi` and `cross` (in case your platform is not `arm` based). So, for let say Ubuntu x86_64 GNU/Linux the right file should be `arm-linux-musleabi-cross.tgz`

### download and extract the tools in a folder

```bash
curl -O https://musl.cc/arm-linux-musleabi-cross.tgz
tar xzf arm-linux-musleabi-cross.tgz
```

### check the version to confirm it works

```bash
./arm-linux-musleabi-cross/bin/arm-linux-musl-gcc --version
```

### compile some projects with a makefile setup

```bash
make CC=./arm-linux-musleabi-cross/bin/arm-linux-musl-gcc LDFLAGS=-static
```

### strip the compiled executable file webfsd

```bash
./arm-linux-musleabi-cross/arm-linux-musleabi/bin/strip webfsd
```

### clean the build

```bash
cd src
make clean
```

### build the webserver executable webfsd

```bash
cd src
make
```

### or build and pack the final package

```bash
./build.sh
```
