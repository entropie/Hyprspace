#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/shared/animation/AnimationTree.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/state/MonitorState.hpp>

CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) {
    ownerID = inOwnerID;

    curAnimationConfig = *Config::animationTree()->getAnimationPropertyConfig("windows");

    // the fuck is pValues???
    curAnimation = *curAnimationConfig.pValues.lock();
    *curAnimationConfig.pValues.lock() = curAnimation;

    if (config.overrideAnimSpeed->value() > 0)
        curAnimation.internalSpeed = config.overrideAnimSpeed->value();

    Animation::mgr()->createAnimation(0.F, curYOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(0.F, workspaceScrollOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    curYOffset->setValueAndWarp(config.panelHeight->value());
    workspaceScrollOffset->setValueAndWarp(0);
}

// TODO: implement deconstructor and delete widget on monitor unplug
CHyprspaceWidget::~CHyprspaceWidget() {}

PHLMONITOR CHyprspaceWidget::getOwner() {
    return State::monitorState()->query().id(ownerID).run();
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner) return;

    if (prevFullscreen.empty()) {
        // unfullscreen all windows
        for (auto& ws : State::workspaceState()->workspaces()) {
            if (ws && ws->m_monitor && ws->m_monitor->m_id == ownerID) {
                const auto w = ws->getFullscreenWindow();
                if (w != nullptr && ws->m_fullscreenMode != FSMODE_NONE) {
                    // use fakefullscreenstate to preserve client's internal state
                    // fixes youtube fullscreen not restoring properly
                    if (ws->m_fullscreenMode == FSMODE_FULLSCREEN) w->m_wantsInitialFullscreen = true;
                    prevFullscreen.emplace_back(std::make_tuple(PHLWINDOWREF(w), ws->m_fullscreenMode));
                    g_pCompositor->setWindowFullscreenState(w, Desktop::View::SFullscreenState{.internal = FSMODE_NONE, .client = FSMODE_NONE});
                }
            }
        }
    }

    // hide top and overlay layers
    // FIXME: ensure input is disabled for hidden layers
    if (oLayerAlpha.empty() && config.hideRealLayers->value()) {
        for (auto& ls : owner->m_layerSurfaceLayers[2]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->alpha()[Desktop::View::LS_ALPHA_FADE]->goal()));
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 0.f;
        }
        for (auto& ls : owner->m_layerSurfaceLayers[3]) {
            //ls->startAnimation(false);
            oLayerAlpha.emplace_back(std::make_tuple(ls.lock(), ls->alpha()[Desktop::View::LS_ALPHA_FADE]->goal()));
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 0.f;
        }
    }

    active = true;

    // panel offset should be handled by swipe event when swiping
    if (!swiping) {
        *curYOffset = 0;
        curSwipeOffset = (config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale;
    }

    updateLayout();
    g_pHyprRenderer->damageMonitor(owner);
    owner->scheduleFrame();
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner) return;

    // restore layer state
    for (auto& ls : owner->m_layerSurfaceLayers[2]) {
        if (ls->m_mapped) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls;});
            if (oAlpha != oLayerAlpha.end()) {
                *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
    }
    for (auto& ls : owner->m_layerSurfaceLayers[3]) {
        if (ls->m_mapped) {
            auto oAlpha = std::find_if(oLayerAlpha.begin(), oLayerAlpha.end(), [&] (const auto& tuple) {return std::get<0>(tuple) == ls;});
            if (oAlpha != oLayerAlpha.end()) {
                *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = std::get<1>(*oAlpha);
            }
            //ls->startAnimation(true);
        }
    }
    oLayerAlpha.clear();

    // restore fullscreen state
    for (auto& fs : prevFullscreen) {
        const auto w = std::get<0>(fs).lock();
        if (!w) continue;
        const auto oFullscreenMode = std::get<1>(fs);
        g_pCompositor->setWindowFullscreenState(w, Desktop::View::SFullscreenState(oFullscreenMode));
        if (oFullscreenMode == FSMODE_FULLSCREEN) w->m_wantsInitialFullscreen = false;
    }
    prevFullscreen.clear();

    active = false;

    // panel offset should be handled by swipe event when swiping
    if (!swiping) {
        *curYOffset = (config.panelHeight->value() + config.reservedArea->value()) * owner->m_scale;
        curSwipeOffset = -10.;
    }

    updateLayout();
    owner->scheduleFrame();
}

void CHyprspaceWidget::updateConfig() {
    curAnimationConfig = *Config::animationTree()->getAnimationPropertyConfig("windows");

    // the fuck is pValues???
    curAnimation = *curAnimationConfig.pValues.lock();
    *curAnimationConfig.pValues.lock() = curAnimation;

    if (config.overrideAnimSpeed->value() > 0)
        curAnimation.internalSpeed = config.overrideAnimSpeed->value();

    Animation::mgr()->createAnimation(0.F, curYOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(0.F, workspaceScrollOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    curYOffset->setValueAndWarp(config.panelHeight->value());
    workspaceScrollOffset->setValueAndWarp(0);
}

bool CHyprspaceWidget::isActive() {
    return active;
}
