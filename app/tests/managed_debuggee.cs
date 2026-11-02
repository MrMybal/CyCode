using System;
using System.Runtime.CompilerServices;
using System.Threading;

public sealed class Box { public int Value = 7; }
public static class Program
{
    [MethodImpl(MethodImplOptions.NoInlining)]
    public static int Increment(int value)
    {
        return value + 1;
    }

    public static int Main(string[] args)
    {
        Console.OutputEncoding = new System.Text.UTF8Encoding(false);
        if (args.Length > 0 && args[0] == "--loop")
        {
            Console.WriteLine("READY");
            while (true) Thread.Sleep(30);
        }
        if (args.Length > 0 && args[0] == "--input")
        {
            Console.WriteLine("INPUT_READY");
            Console.WriteLine("INPUT=" + Console.ReadLine());
            return 0;
        }
        int value = 41;
        Box model = new Box();
        int[] values = new int[] { 10, 11, 12, 13, 14 };
        value = Increment(value); // BREAK_HERE
        Console.WriteLine("RESULT=" + value + " ARG=" + (args.Length > 0 ? args[0] : "none")
            + " ENV=" + Environment.GetEnvironmentVariable("CYCODE_DEBUG_MARKER"));
        GC.KeepAlive(model);
        GC.KeepAlive(values);
        return value == 42 ? 0 : 1;
    }
}
