/* Test-only native entry point for an actual COBOL 4J generated Java class.
 * Absolute class paths keep Beryl's private working directory independent of
 * the compiler output directory. exec preserves JVM exit/signal semantics. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(void) {
    const char *classpath=getenv("BERYL_TEST_CLASSPATH");
    if(!classpath)classpath=BERYL_JAVA_CLASSPATH;
    execl(BERYL_JAVA_EXECUTABLE,BERYL_JAVA_EXECUTABLE,"-Xms16m","-Xmx64m",
        "-XX:+UseSerialGC","-XX:MaxMetaspaceSize=64m",
        "-XX:CompressedClassSpaceSize=16m","-XX:ReservedCodeCacheSize=32m",
        "-Xss256k","-XX:ActiveProcessorCount=2","-cp",classpath,
        BERYL_JAVA_CLASS,(char *)NULL);
    perror("Java fixture exec");return 127;
}
