PREFIX?=/data/data/com.termux/files/usr
CC_ARM64:=$(PREFIX)/bin/aarch64-linux-android-clang
AAPT:=$(PREFIX)/bin/aapt
ZIPALIGN:=$(PREFIX)/bin/zipalign
APKSIGNER:=$(PREFIX)/bin/apksigner
KOTLINC := $(PREFIX)/bin/kotlinc
D8 := $(PREFIX)/bin/d8

# Paths
BUILD_DIR?=.
KOTLIN_SRC := src/bosun/ops/BosunActivity.kt
KOTLIN_JAR = $(BUILD_DIR)/kotlin_out.jar
DEX_OUT = $(BUILD_DIR)/classes.dex
PACKAGENAME?=bosun.ops
APPNAME?=bosun
LABEL?=Bosun
ANDROIDVERSION?=26
ANDROIDTARGET?=$(ANDROIDVERSION)
ANDROID_JAR := android-$(ANDROIDVERSION).jar

# For compilation we use a fixed API level (e.g., 30) to get modern Kotlin stdlib support
ANDROIDVERSION_COMPILE := 30
ANDROID_JAR_COMPILE := android-30.jar

RAWDRAWANDROID?=.
RAWDRAWANDROIDSRCS=$(RAWDRAWANDROID)/android_native_app_glue.c
SRC?=test.c
ANDROIDSRCS:= $(SRC) $(RAWDRAWANDROIDSRCS)

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

# -------------------------------------------------------------------
# Targets
# -------------------------------------------------------------------
all: bosun001.apk

.PHONY: clean keystore

# -------------------------------------------------------------------
# Download android.jar for target API (for aapt and d8)
# -------------------------------------------------------------------
$(ANDROID_JAR):
	wget https://github.com/Sable/android-platforms/raw/master/android-$(ANDROIDVERSION)/android.jar -O $(ANDROID_JAR) || { echo "Failed to download $(ANDROID_JAR)"; exit 1; }

$(ANDROID_JAR_COMPILE):
	@echo "Downloading platform-30 for compilation..."
	curl -L -f -S https://dl.google.com/android/repository/platform-30_r03.zip -o platform-30.zip || { echo "Failed to download platform-30.zip"; exit 1; }
	unzip -o -j platform-30.zip "android-11/android.jar" -d . || { echo "Failed to unzip android.jar"; exit 1; }
	mv android.jar $(ANDROID_JAR_COMPILE)
	rm -f platform-30.zip

# -------------------------------------------------------------------
# Keystore for signing
# -------------------------------------------------------------------
$(KEYSTOREFILE):
	keytool -genkey -v -keystore $(KEYSTOREFILE) -alias $(ALIASNAME) -keyalg RSA -keysize 2048 -validity 10000 -storepass $(STOREPASS) -keypass $(STOREPASS) -dname $(DNAME)

# -------------------------------------------------------------------
# Native library
# -------------------------------------------------------------------
$(BUILD_DIR)/lib/arm64-v8a/lib$(APPNAME).so: $(ANDROIDSRCS)
	mkdir -p $(BUILD_DIR)/lib/arm64-v8a
	$(CC_ARM64) $(CFLAGS) -m64 -o $@ $^ $(LDFLAGS) || { echo "Native library build failed"; exit 1; }

# -------------------------------------------------------------------
# Resource compilation and R.java generation
# -------------------------------------------------------------------
# Compile resources to .flat files
COMPILED_RES := $(BUILD_DIR)/compiled_res
$(COMPILED_RES)/%.flat: Sources/res/%
	mkdir -p $(COMPILED_RES)
	aapt2 compile -o $(COMPILED_RES) $< || { echo "aapt2 compile failed for $<"; exit 1; }

# We need to collect all resource files. This is a simplified rule – 
# for a real project you'd use a wildcard or a list. Here we assume
# you have a single directory; aapt2 compile can process a whole dir.
# Let's use a simpler approach: compile the whole res directory at once.
$(COMPILED_RES)/all.flags: Sources/res
	mkdir -p $(COMPILED_RES)
	aapt2 compile --dir Sources/res -o $(COMPILED_RES)/ || { echo "aapt2 compile of res dir failed"; exit 1; }
	touch $(COMPILED_RES)/all.flags

# Link resources, generate R.java, and produce a temporary APK without code
TEMP_APK_NOCODE := $(BUILD_DIR)/temp_nocode.apk
R_JAVA_DIR := src  # where aapt2 will place R.java
$(TEMP_APK_NOCODE) $(R_JAVA_DIR)/bosun/ops/R.java: $(ANDROID_JAR) $(BUILD_DIR)/AndroidManifest.xml $(COMPILED_RES)/all.flags
	mkdir -p $(R_JAVA_DIR)
	aapt2 link -o $(TEMP_APK_NOCODE) \
		-I $(ANDROID_JAR) \
		--manifest $(BUILD_DIR)/AndroidManifest.xml \
		-R $(COMPILED_RES)/*.flat \
		--java $(R_JAVA_DIR) || { echo "aapt2 link failed"; exit 1; }

# -------------------------------------------------------------------
# Kotlin compilation (needs R.java on classpath)
# -------------------------------------------------------------------
$(KOTLIN_JAR): $(KOTLIN_SRC) $(ANDROID_JAR_COMPILE) $(R_JAVA_DIR)/bosun/ops/R.java
	mkdir -p $(BUILD_DIR)
	$(KOTLINC) $< \
		-cp $(ANDROID_JAR_COMPILE):$(R_JAVA_DIR) \
		-jvm-target 1.8 \
		-no-jdk -no-reflect -include-runtime \
		-d $@ || { echo "Kotlin compilation failed"; exit 1; }

# -------------------------------------------------------------------
# Dexing
# -------------------------------------------------------------------
$(DEX_OUT): $(KOTLIN_JAR) $(ANDROID_JAR)
	$(D8) --release --lib $(ANDROID_JAR) --output $(BUILD_DIR) $< || { echo "D8 dexing failed"; exit 1; }

# -------------------------------------------------------------------
# Generate AndroidManifest.xml from template
# -------------------------------------------------------------------
$(BUILD_DIR)/AndroidManifest.xml: AndroidManifest.xml.template
	mkdir -p $(BUILD_DIR)
	PACKAGENAME=$(PACKAGENAME) \
	ANDROIDVERSION=$(ANDROIDVERSION) \
	ANDROIDTARGET=$(ANDROIDTARGET) \
	APPNAME=$(APPNAME) \
	LABEL=$(LABEL) envsubst '$$ANDROIDTARGET $$ANDROIDVERSION $$APPNAME $$PACKAGENAME $$LABEL' \
		< $< > $@ || { echo "Manifest generation failed"; exit 1; }

# -------------------------------------------------------------------
# Final APK assembly
# -------------------------------------------------------------------
# Temporary APK with resources but no code
TEMP_APK := $(BUILD_DIR)/temp.apk
UNPACK_DIR := $(BUILD_DIR)/makecapk

# Combine resource APK with dex and native lib
$(TEMP_APK): $(TEMP_APK_NOCODE) $(DEX_OUT) $(BUILD_DIR)/lib/arm64-v8a/lib$(APPNAME).so
	cp $(TEMP_APK_NOCODE) $(TEMP_APK)
	# Add classes.dex
	zip -j $(TEMP_APK) $(DEX_OUT) || { echo "Failed to add dex to APK"; exit 1; }
	# Add native library
	mkdir -p $(UNPACK_DIR)/lib/arm64-v8a
	cp $(BUILD_DIR)/lib/arm64-v8a/lib$(APPNAME).so $(UNPACK_DIR)/lib/arm64-v8a/
	# Also add any assets
	mkdir -p $(UNPACK_DIR)/assets
	cp -r Sources/assets/* $(UNPACK_DIR)/assets/ 2>/dev/null || true
	# Update the APK with the new directories
	(cd $(UNPACK_DIR) && zip -r ../$(TEMP_APK) .) || { echo "Failed to update APK"; exit 1; }

# Align and sign
bosun001.apk: $(TEMP_APK) $(KEYSTOREFILE)
	$(ZIPALIGN) -f -v -p 4 $(TEMP_APK) bosun001-unaligned.apk || { echo "Zipalign failed"; exit 1; }
	$(APKSIGNER) sign --ks $(KEYSTOREFILE) \
		--ks-pass pass:$(STOREPASS) \
		--min-sdk-version 23 \
		--v1-signing-enabled true \
		--v2-signing-enabled true \
		--v3-signing-enabled true \
		--out bosun001.apk bosun001-unaligned.apk || { echo "Signing failed"; exit 1; }
	@echo "APK created: bosun001.apk"
	# Optionally copy to downloads
	cp bosun001.apk ~/storage/downloads/ 2>/dev/null || true

# -------------------------------------------------------------------
# Clean
# -------------------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR)/*.jar $(BUILD_DIR)/*.dex $(BUILD_DIR)/*.apk $(BUILD_DIR)/temp_nocode.apk $(BUILD_DIR)/compiled_res $(BUILD_DIR)/makecapk $(BUILD_DIR)/lib bosun001.apk bosun001-unaligned.apk
	rm -f $(ANDROID_JAR) $(ANDROID_JAR_COMPILE) platform-30.zip
