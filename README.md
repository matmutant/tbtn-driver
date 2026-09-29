# tbtn-driver

## FZ-M1 specific adaptation
### How Button A is seen by X
```
Button A keypress
↓
ACPI MAT0035 event
↓
[tbtn driver]
↓
KEY-PROG1 (Linux Code 148)
↓
XF86Launch1 (X11 keycode 156)
```


## How to install

```
make
```

then

### temporary installation

```
sudo insmod tbtn_driver.ko
```

if you want to remove:

```
sudo rmmod tbtn_driver.ko
```
### autoload 
prepare directory and copy made module
```
sudo mkdir -p /lib/modules/$(uname -r)/extra
```
```
sudo cp tbtn_driver.ko /lib/modules/$(uname -r)/extra/
```
refresh module index:
```
sudo depmod -a
```
test module loading:
```
sudo modprobe tbtn_driver
```
add conf 
```
sudo vim /etc/modules-load.d/tbtn.conf
[i]
tbtn_driver
[esc] :wq
```


### alternative installation

```
make install
```
