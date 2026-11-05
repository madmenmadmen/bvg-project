plugins {
    `maven-publish`
    signing
    id("tech.yanand.maven-central-publish") version "1.3.0"
}

dependencies {
    implementation("com.github.luben:zstd-jni:1.5.6-4")
    implementation("org.lz4:lz4-java:1.8.0")
}

java {
    withJavadocJar()
    withSourcesJar()
}

publishing {
    publications {
        create<MavenPublication>("mavenJava") {
            artifactId = "bvg"
            from(components["java"])
            pom {
                name.set("BVG - Binary Vector Graphics")
                description.set("BVG Java library (core)")
                url.set("https://github.com/madmenmadmen/bvg-project")
                licenses {
                    license {
                        name.set("MIT License")
                        url.set("https://opensource.org/licenses/MIT")
                    }
                }
                developers {
                    developer {
                        id.set("madmenmadmen")
                        name.set("madmenmadmen")
                    }
                }
                scm {
                    connection.set("scm:git:git://github.com/madmenmadmen/bvg-project.git")
                    url.set("https://github.com/madmenmadmen/bvg-project")
                }
            }
        }
    }
}

signing {
    useGpgCmd()
    sign(publishing.publications["mavenJava"])
}

mavenCentral {
    val t = System.getenv("CENTRAL_AUTH_BASE64")
    authToken.set(t)
    publishingType.set("AUTOMATIC")
    maxWait.set(120)
}