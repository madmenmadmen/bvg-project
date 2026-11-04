plugins {
    application
    java
    id("org.openjfx.javafxplugin") version "0.1.0" // JavaFX plugin
    id("com.gradleup.shadow") version "9.0.0-beta12"
}

group = "io.bvg"
version = "1.0.0"

repositories {
    mavenCentral()
}

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(17))
    }
}

javafx {
    version = "21" // LTS
    modules = listOf("javafx.controls", "javafx.graphics", "javafx.swing")
}

dependencies {
    implementation("com.github.luben:zstd-jni:1.5.6-4")
    implementation("org.lz4:lz4-java:1.8.0")

    // 🎨 Современный UI (тёмная тема)
    implementation("io.github.mkpaz:atlantafx-base:2.0.1")

    testImplementation("org.junit.jupiter:junit-jupiter:5.10.0")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher:1.10.0")
}

application {
    mainClass.set("io.bvg.viewer.JfxViewer")
}

// CLI jar (Main = io.bvg.Main)
tasks.named<com.github.jengelman.gradle.plugins.shadow.tasks.ShadowJar>("shadowJar") {
    archiveBaseName.set("bvg-java-cli")
    archiveClassifier.set("")
    archiveVersion.set("")
    manifest { attributes["Main-Class"] = "io.bvg.Main" }
    mergeServiceFiles()
}

tasks.register("cliJar") {
    dependsOn("shadowJar")
    doLast { println("built: build/libs/bvg-java-cli.jar") }
}

// Viewer fat-jar (Main = io.bvg.viewer.JfxViewer)
tasks.register<com.github.jengelman.gradle.plugins.shadow.tasks.ShadowJar>("viewerJar") {
    archiveBaseName.set("bvg-java-viewer")
    archiveClassifier.set("")
    archiveVersion.set("")
    manifest { attributes["Main-Class"] = "io.bvg.viewer.Launcher" }
    mergeServiceFiles()
    from(sourceSets.main.get().output)
    configurations = listOf(project.configurations.runtimeClasspath.get())
}

tasks.test {
    useJUnitPlatform()
}