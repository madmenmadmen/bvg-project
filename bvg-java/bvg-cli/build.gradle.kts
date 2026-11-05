plugins {
    application
    id("com.gradleup.shadow")
}

dependencies {
    implementation(project(":bvg-core"))
}

application {
    mainClass.set("io.bvg.Main")
}

tasks.named<com.github.jengelman.gradle.plugins.shadow.tasks.ShadowJar>("shadowJar") {
    archiveBaseName.set("bvg-java-cli")
    archiveClassifier.set("")
    archiveVersion.set("")
    manifest { attributes["Main-Class"] = "io.bvg.Main" }
    mergeServiceFiles()
}

tasks.register("cliJar") {
    dependsOn("shadowJar")
    doLast { println("built: bvg-cli/build/libs/bvg-java-cli.jar") }
}