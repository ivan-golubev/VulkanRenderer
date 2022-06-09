export module GlobalSettings;

export namespace gg 
{
    inline constexpr bool IsDebug() 
    {
#ifdef _DEBUG
        return true;
#else
        return false;
#endif
    }

    inline constexpr bool IsFinal()
    {
#ifdef FINAL
        return true;
#else
        return false;
#endif
    }

    inline constexpr bool IsWindowsSubSystem()
    {
#ifdef _CONSOLE
        return false;
#else
        return true;
#endif
    }
} // namespace gg
