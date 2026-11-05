plugins {
    java
    `maven-publish`
    signing
    id("org.openjfx.javafxplugin") version "0.1.0" apply false
    id("com.gradleup.shadow") version "9.0.0-beta12" apply false
}

allprojects {
    group = "io.github.madmenmadmen"
    version = "1.0.0"

    repositories {
        mavenCentral()
    }
}

subprojects {
    apply(plugin = "java")
    apply(plugin = "maven-publish")
    apply(plugin = "signing")

    java {
        toolchain {
            languageVersion.set(JavaLanguageVersion.of(17))
        }
        withSourcesJar()
        withJavadocJar()
    }

    tasks.withType<Javadoc> {
        (options as StandardJavadocDocletOptions).addStringOption("Xdoclint:none", "-quiet")
    }
}