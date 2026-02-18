using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace AltinaEngine.Managed;

// Delegate type for hostfxr load_assembly_and_get_function_pointer
[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public unsafe delegate IntPtr ManagedStartupDelegate(IntPtr nativeApi, int nativeApiSize);

public static unsafe class ManagedBootstrap
{
    private static readonly Dictionary<ulong, ScriptComponent> sInstances = new();
    private static readonly Dictionary<string, Assembly> sLoadedAssemblies =
        new(StringComparer.OrdinalIgnoreCase);
    private static IntPtr sManagedApiPtr = IntPtr.Zero;
    private static ulong sNextHandle = 1;
    private static bool sResolverInstalled = false;

    public static IntPtr Startup(IntPtr nativeApi, int nativeApiSize)
    {
        if (nativeApi == IntPtr.Zero || nativeApiSize < sizeof(NativeApi))
        {
            return IntPtr.Zero;
        }

        NativeApi api = *(NativeApi*)nativeApi;
        Native.SetApi(api);
        Native.LogInfo("Managed scripting runtime startup initialized.");

        if (!sResolverInstalled)
        {
            AssemblyLoadContext.Default.Resolving += ResolveAssembly;
            sResolverInstalled = true;
            Native.LogInfo($"Managed assembly resolver installed. BaseDir='{GetBaseDirectory()}'");
        }

        if (sManagedApiPtr == IntPtr.Zero)
        {
            ManagedApi managedApi = new()
            {
                CreateInstance = &CreateInstance,
                DestroyInstance = &DestroyInstance,
                OnCreate = &OnCreate,
                OnDestroy = &OnDestroy,
                OnEnable = &OnEnable,
                OnDisable = &OnDisable,
                Tick = &Tick,
            };

            sManagedApiPtr = Marshal.AllocHGlobal(sizeof(ManagedApi));
            *(ManagedApi*)sManagedApiPtr = managedApi;
        }

        return sManagedApiPtr;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static ulong CreateInstance(ManagedCreateArgs* args)
    {
        if (args == null)
        {
            return 0;
        }

        string? typeName = Utf8ToString(args->TypeNameUtf8);
        if (string.IsNullOrEmpty(typeName))
        {
            Native.LogError("Managed CreateInstance failed: missing type name.");
            return 0;
        }

        string? assemblyPath = Utf8ToString(args->AssemblyPathUtf8);
        Type? type = ResolveType(typeName, assemblyPath);
        if (type == null)
        {
            Native.LogError($"Managed CreateInstance failed: type '{typeName}' not found.");
            return 0;
        }

        if (!typeof(ScriptComponent).IsAssignableFrom(type))
        {
            Native.LogError($"Managed CreateInstance failed: type '{typeName}' is not a ScriptComponent.");
            return 0;
        }

        ScriptComponent instance;
        try
        {
            instance = (ScriptComponent)Activator.CreateInstance(type)!;
        }
        catch (Exception ex)
        {
            Native.LogError($"Managed CreateInstance failed: {ex.GetType().Name}: {ex.Message}");
            return 0;
        }

        ulong handle = sNextHandle++;
        instance.InstanceHandle = handle;
        instance.OwnerIndex = args->OwnerIndex;
        instance.OwnerGeneration = args->OwnerGeneration;
        instance.WorldId = args->WorldId;

        sInstances[handle] = instance;
        return handle;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void DestroyInstance(ulong handle)
    {
        sInstances.Remove(handle);
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void OnCreate(ulong handle) => Invoke(handle, static c => c.OnCreate());

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void OnDestroy(ulong handle) => Invoke(handle, static c => c.OnDestroy());

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void OnEnable(ulong handle) => Invoke(handle, static c => c.OnEnable());

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void OnDisable(ulong handle) => Invoke(handle, static c => c.OnDisable());

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void Tick(ulong handle, float dt)
    {
        if (!sInstances.TryGetValue(handle, out ScriptComponent? instance))
        {
            return;
        }

        try
        {
            instance.Tick(dt);
        }
        catch (Exception ex)
        {
            Native.LogError($"Managed script exception: {ex.GetType().Name}: {ex.Message}");
        }
    }

    private static void Invoke(ulong handle, Action<ScriptComponent> action)
    {
        if (!sInstances.TryGetValue(handle, out ScriptComponent? instance))
        {
            return;
        }

        try
        {
            action(instance);
        }
        catch (Exception ex)
        {
            Native.LogError($"Managed script exception: {ex.GetType().Name}: {ex.Message}");
        }
    }

    private static Type? ResolveType(string typeName, string? assemblyPath)
    {
        string? simpleTypeName = typeName;
        string? assemblyName = null;
        int commaIndex = typeName.IndexOf(',');
        if (commaIndex >= 0)
        {
            simpleTypeName = typeName[..commaIndex].Trim();
            assemblyName = typeName[(commaIndex + 1)..].Trim();
        }

        Type? type = Type.GetType(typeName, throwOnError: false);
        if (type != null)
        {
            return type;
        }

        if (string.IsNullOrEmpty(assemblyPath))
        {
            if (!string.IsNullOrEmpty(assemblyName))
            {
                string candidate = Path.Combine(AppContext.BaseDirectory, assemblyName + ".dll");
                if (File.Exists(candidate))
                {
                    assemblyPath = candidate;
                }
            }
            if (string.IsNullOrEmpty(assemblyPath))
            {
                return null;
            }
        }

        try
        {
            if (!Path.IsPathRooted(assemblyPath))
            {
                assemblyPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, assemblyPath));
            }

            if (!sLoadedAssemblies.TryGetValue(assemblyPath, out Assembly? assembly))
            {
                assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(assemblyPath);
                sLoadedAssemblies[assemblyPath] = assembly;
            }

            type = assembly.GetType(typeName, throwOnError: false);
            if (type != null)
            {
                return type;
            }

            if (!string.IsNullOrEmpty(simpleTypeName) && !string.Equals(simpleTypeName, typeName, StringComparison.Ordinal))
            {
                type = assembly.GetType(simpleTypeName, throwOnError: false);
                if (type != null)
                {
                    return type;
                }
            }

            try
            {
                Type[] types = assembly.GetTypes();
                int count = types.Length;
                int max = Math.Min(count, 16);
                StringBuilder builder = new();
                for (int i = 0; i < max; ++i)
                {
                    if (i > 0)
                    {
                        builder.Append(", ");
                    }
                    builder.Append(types[i].FullName ?? types[i].Name);
                }
                Native.LogInfo(
                    $"Managed ResolveType miss. Assembly='{assembly.FullName}' Location='{assembly.Location}' Types={count} Sample=[{builder}]");
            }
            catch (Exception ex)
            {
                Native.LogError($"Managed ResolveType failed to enumerate types: {ex.GetType().Name}: {ex.Message}");
            }

            return null;
        }
        catch (Exception ex)
        {
            Native.LogError($"Managed ResolveType failed: {ex.GetType().Name}: {ex.Message}");
            return null;
        }
    }

    private static string? Utf8ToString(byte* ptr)
    {
        if (ptr == null)
        {
            return null;
        }

        return Marshal.PtrToStringUTF8((IntPtr)ptr);
    }

    private static Assembly? ResolveAssembly(AssemblyLoadContext context, AssemblyName name)
    {
        if (string.IsNullOrEmpty(name.Name))
        {
            return null;
        }

        foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
        {
            AssemblyName loadedName = assembly.GetName();
            if (string.Equals(loadedName.Name, name.Name, StringComparison.OrdinalIgnoreCase))
            {
                return assembly;
            }
        }

        string baseDir = GetBaseDirectory();
        if (string.IsNullOrEmpty(baseDir))
        {
            return null;
        }

        string candidate = Path.Combine(baseDir, name.Name + ".dll");
        if (!Path.IsPathRooted(candidate))
        {
            candidate = Path.GetFullPath(candidate);
        }

        if (File.Exists(candidate))
        {
            try
            {
                return context.LoadFromAssemblyPath(candidate);
            }
            catch (Exception ex)
            {
                Native.LogError($"Managed ResolveAssembly failed: {name.Name} -> {ex.GetType().Name}: {ex.Message}");
            }
        }

        return null;
    }

    private static string GetBaseDirectory()
    {
        if (!string.IsNullOrEmpty(AppContext.BaseDirectory))
        {
            return AppContext.BaseDirectory;
        }

        if (!string.IsNullOrEmpty(Environment.ProcessPath))
        {
            string? dir = Path.GetDirectoryName(Environment.ProcessPath);
            if (!string.IsNullOrEmpty(dir))
            {
                return dir;
            }
        }

        return Directory.GetCurrentDirectory();
    }
}
