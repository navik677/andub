package ua.andub.tv.data.providers

object ProviderRegistry {

    private val providers = listOf(
        AniTubeProvider(),
        AniLibriaProvider(),
        DreamCastProvider(),
        AniBazaProvider(),
        AniDubProvider(),
        ShizaProjectProvider(),
        AnimeVostProvider()
    )

    private val providerMap = providers.associateBy { it.name }

    var currentProvider: BaseProvider = providers[0] // AniTube is default

    fun getProviders(): List<BaseProvider> = providers

    fun getProvider(name: String): BaseProvider {
        return providers.firstOrNull {
            it.name.equals(name, ignoreCase = true) || it.displayName.equals(name, ignoreCase = true)
        } ?: providers[0]
    }

    fun setProvider(name: String) {
        val found = providers.firstOrNull {
            it.name.equals(name, ignoreCase = true) || it.displayName.equals(name, ignoreCase = true)
        }
        if (found != null) {
            currentProvider = found
        }
    }
}
