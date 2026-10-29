// JVM test for the language-set selection: javac -d /tmp/ls android/app/src/main/java/io/github/sirbilliams/rollerbot/LangSets.java tests/lang/LangSetsTest.java
// && java -cp /tmp/ls io.github.sirbilliams.rollerbot.LangSetsTest
package io.github.sirbilliams.rollerbot;

public class LangSetsTest {
    static int fails;
    static void eq(Object a, Object b, String what) { if (!java.util.Objects.equals(a, b)) { fails++; System.err.println("FAIL " + what + ": " + a + " != " + b); } }
    public static void main(String[] x) {
        eq(LangSets.langOf("Disc/CodeAndData(French)/a/b.dat"), "french", "langOf");
        eq(LangSets.langOf("game/codeanddata(english)/Rollerbot.exe"), "english", "langOf lower");
        eq(LangSets.langOf("CodeAndData(klingon)/a"), null, "unknown language");
        eq(LangSets.langOf("Rollerbot.exe"), null, "plain");
        eq(LangSets.relAfter("Disc\\CodeAndData(german)\\a\\b.dat"), "a/b.dat", "relAfter");
        eq(LangSets.relAfter("a/b.dat"), "a/b.dat", "relAfter plain");
        // two sets in one source: each file goes to its own language, never to the other
        eq(LangSets.langOf("CodeAndData(english)/marker.txt"), "english", "set 1");
        eq(LangSets.langOf("CodeAndData(french)/marker.txt"), "french", "set 2");
        eq(LangSets.active(1, new boolean[] { true, true, false, false, false }), 1, "chosen installed");
        eq(LangSets.active(2, new boolean[] { false, true, false, false, true }), 1, "chosen missing: first installed");
        eq(LangSets.active(0, new boolean[5]), 0, "none installed");
        eq(LangSets.missingMessage(new String[] { "EN", "FR", "DE", "ES", "IT" }, 0, new boolean[] { false, true, true, false, false }).contains("FR, DE"), true, "message lists languages");
        if (fails == 0) System.out.println("LangSetsTest: ok"); else System.exit(1);
    }
}
