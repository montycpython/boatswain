PREFIX?=/data/data/com.termux/files/usr
CC_ARM64:=$(PREFIX)/bin/aarch64-linux-android-clang
AAPT:=$(PREFIX)/bin/aapt
ZIPALIGN:=$(PREFIX)/bin/zipalign
APKSIGNER:=$(PREFIX)/bin/apksigner

PACKAGENAME?=bosun.ops
APPNAME?=bosun
LABEL?=Bosun
ANDROIDVERSION?=23
ANDROIDTARGET?=$(ANDROIDVERSION)
RAWDRAWANDROID?=.
RAWDRAWANDROIDSRCS=$(RAWDRAWANDROID)/android_native_app_glue.c
SRC?=test.c
ANDROIDSRCS:= $(SRC) $(RAWDRAWANDROIDSRCS)
BUILD_DIR?=.

CFLAGS?=-ffunction-sections -Os -fdata-sections -Wall -fvisibility=hidden
CFLAGS += -target aarch64-linux-android$(ANDROIDVERSION) -DACK_PVT_FIELDS
CFLAGS += -DANDROID -DAPPNAME=\"$(APPNAME)\" -DANDROIDVERSION=$(ANDROIDVERSION)
CFLAGS += -I. -I$(RAWDRAWANDROID)/rawdraw -I$(PREFIX)/include -fPIC
LDFLAGS += -lm -lGLESv3 -lEGL -landroid -llog -lOpenSLES
LDFLAGS += -shared -uANativeActivity_onCreate -L$(PREFIX)/lib -s

STOREPASS?=montavious
KEYSTOREFILE:=my-release-key.keystore
ALIASNAME?=standkey
DNAME:="CN=$(APPNAME), O=$(APPNAME), C=US"

all: bosun000.apk

.PHONY: clean keystore

ANDROID_JAR:=android-$(ANDROIDVERSION).jar

$(ANDROID_JAR):
	wget https://github.com/Sable/android-platforms/raw/master/android-$(ANDROIDVERSION)/android.jar -O $(ANDROID_JAR)

$(KEYSTOREFILE):
	keytool -genkey -v -keystore $(KEYSTOREFILE) -alias $(ALIASNAME) -keyalg RSA -keysize 2048 -validity 10000 -storepass $(STOREPASS) -keypass $(STOREPASS) -dname $(DNAME)

$(BUILD_DIR)/makecapk/lib/arm64-v8a/lib$(APPNAME).so: $(ANDROIDSRCS)
	mkdir -p $(BUILD_DIR)/makecapk/lib/arm64-v8a
	$(CC_ARM64) $(CFLAGS) -m64 -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/AndroidManifest.xml: AndroidManifest.xml.template
	mkdir -p $(BUILD_DIR)
	PACKAGENAME=$(PACKAGENAME) \
		ANDROIDVERSION=$(ANDROIDVERSION) \
		ANDROIDTARGET=$(ANDROIDTARGET) \
		APPNAME=$(APPNAME) \
		LABEL=$(LABEL) envsubst '$$ANDROIDTARGET $$ANDROIDVERSION $$APPNAME $$PACKAGENAME $$LABEL' \
		< AndroidManifest.xml.template > $(BUILD_DIR)/AndroidManifest.xml

bosun000.apk: $(BUILD_DIR)/makecapk/lib/arm64-v8a/lib$(APPNAME).so $(BUILD_DIR)/AndroidManifest.xml $(KEYSTOREFILE) $(ANDROID_JAR)
	mkdir -p $(BUILD_DIR)/makecapk/assets
	cp -r Sources/assets/* $(BUILD_DIR)/makecapk/assets || mkdir -p $(BUILD_DIR)/makecapk/assets
	rm -f temp.apk
	mkdir -p compiled_res
	aapt2 compile --dir Sources/res -o compiled_res/
	aapt2 link -o temp.apk -I $(ANDROID_JAR) --manifest AndroidManifest.xml -R compiled_res/*.flat --java src/
	rm -f makecapk.apk
	mkdir -p makecapk
	cd makecapk && unzip -o ../temp.apk
	cd makecapk && zip -D4r ../makecapk.apk .
	$(ZIPALIGN) -f -v 4 makecapk.apk bosun000.apk
	$(APKSIGNER) sign --ks $(KEYSTOREFILE) \
		--ks-pass pass:$(STOREPASS) \
		--min-sdk-version 23 \
		--v1-signing-enabled true \
		--v2-signing-enabled true \
		--v3-signing-enabled true \
		--out bosun000.apk bosun000.apk
	mv bosun000.apk ~/storage/downloads/

clean:
	rm -rf makecapk AndroidManifest.xml bosun000.apk temp.apk makecapk.apk compiled_res

