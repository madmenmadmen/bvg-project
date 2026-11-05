plugins {
    application
    id("org.openjfx.javafxplugin")
    id("com.gradleup.shadow")
}

javafx {
    version = "21"
    modules = listOf("javafx.controls", "javafx.graphics", "javafx.swing")
}

dependencies {
    implementation(project(":bvg-core"))
    implementation("io.github.mkpaz:atlantafx-base:2.0.1")
}

application {
    mainClass.set("io.bvg.viewer.Launcher")
}

tasks.named<com.github.jengelman.gradle.plugins.shadow.tasks.ShadowJar>("shadowJar") {
    archiveBaseName.set("bvg-java-viewer")
    archiveClassifier.set("")
    archiveVersion.set("")
    manifest { attributes["Main-Class"] = "io.bvg.viewer.Launcher" }
    mergeServiceFiles()
}

tasks.register("viewerJar") {
    dependsOn("shadowJar")
    doLast { println("built: bvg-viewer/build/libs/bvg-java-viewer.jar") }
}