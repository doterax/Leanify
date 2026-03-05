async function loadCCSettings(baseUrl) {
    const settingsJsonUrl = `${baseUrl}res/settings.json`;
    console.log(`[Loader] start loading CCSettings from url: ${settingsJsonUrl}`);
    const settingsResponse = await fetch(settingsJsonUrl);
    console.log(`[Loader] loading CCSettings completed`);

    const result = await settingsResponse.json();
    console.log(`[Loader] CCSettings successfully parsed`);

    return result;
}

async function initializeCCSettings()
{
    console.log(`[Loader] Initializing Cocos settings ...`);
    var baseUrl = "./";
    if (window.noAssets) {
        console.log(`[Loader] Working with external assets ...`);
        baseUrl = window.assetsBaseUrl;
        console.log(`[Loader] Resources base is: ${baseUrl}`);
    }
    const settings = await window.loadCCSettings(baseUrl);
    window._CCSettings = settings;
}

window.initializeCCSettings = initializeCCSettings;