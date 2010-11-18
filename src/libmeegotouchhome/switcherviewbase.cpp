/***************************************************************************
 **
 ** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 ** All rights reserved.
 ** Contact: Nokia Corporation (directui@nokia.com)
 **
 ** This file is part of mhome.
 **
 ** If you have questions regarding the use of this file, please contact
 ** Nokia at directui@nokia.com.
 **
 ** This library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public
 ** License version 2.1 as published by the Free Software Foundation
 ** and appearing in the file LICENSE.LGPL included in the packaging
 ** of this file.
 **
 ****************************************************************************/
#include <MCancelEvent>
#include "switcherviewbase.h"
#include "switcher.h"
#include "switcherbutton.h"
#include "transformlayoutanimation.h"
#include "mainwindow.h"

#include <MScene>
#include <MSceneManager>
#include <MViewCreator>
#include <MLayout>
#include <MTheme>
#include <MLocale>
#include <MApplication>
#include <MWindow>
#include <MDeviceProfile>
#include <QGraphicsLinearLayout>
#include <QPinchGesture>
#include <MPannableViewport>
#include <math.h>
#include <algorithm>
#include <cfloat>
#include <QGestureEvent>
#include <QPropertyAnimation>


SwitcherViewBase::SwitcherViewBase(Switcher *switcher) :
        MWidgetView(switcher), controller(switcher), mainLayout(new QGraphicsLinearLayout(Qt::Vertical)), pannedWidget(new MWidget), pinchedButtonPosition(-1), layoutAnimation(NULL), overpinch(false), animating(false)
{
    mainLayout->setContentsMargins(0, 0, 0, 0);
    switcher->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    switcher->setLayout(mainLayout);

    pannedLayout = new MLayout(pannedWidget);
    pannedLayout->setContentsMargins(0, 0, 0, 0);

    bounceAnimation = new QPropertyAnimation(this);
    bounceAnimation->setTargetObject(pannedWidget);
    bounceAnimation->setPropertyName("scale");
    bounceAnimation->setStartValue(1.0f);
    bounceAnimation->setEndValue(1.0f);
    connect(bounceAnimation, SIGNAL(finished()), this, SLOT(endBounce()));
    connect(bounceAnimation, SIGNAL(stateChanged(QAbstractAnimation::State,QAbstractAnimation::State)), this, SLOT(updateAnimationStatus()));

    connect(this, SIGNAL(animationStateChanged(bool)), switcher, SLOT(updateAnimationStatus(bool)));
}

SwitcherViewBase::~SwitcherViewBase()
{
    removeButtonsFromLayout();
}

void SwitcherViewBase::removeButtonsFromLayout()
{
    // Remove all buttons from the layout and set parents to null (do not destroy them)
    for (int i = 0, count = pannedLayout->count(); i < count; i++) {
        static_cast<SwitcherButton *>(pannedLayout->takeAt(0))->setParentItem(0);
    }
}

bool SwitcherViewBase::event(QEvent *e)
{
    // This stuff is necessary to receive touch events.
    if (e->type() == QEvent::TouchBegin) {
        e->setAccepted(true);
        return true;
    }
    return MWidgetView::event(e);
}

void SwitcherViewBase::setupModel()
{
    MWidgetView::setupModel();
    applySwitcherMode();
}

void SwitcherViewBase::applySwitcherMode()
{
    if (model()->switcherMode() == SwitcherModel::Detailview) {
        controller->setStyleName("DetailviewSwitcher");
    } else {
        controller->setStyleName("OverviewSwitcher");
    }
}

void SwitcherViewBase::updateData(const QList<const char*>& modifications)
{
    MWidgetView::updateData(modifications);
    const char *member;
    foreach(member, modifications) {
        if (member == SwitcherModel::SwitcherMode) {
            applySwitcherMode();
        }
    }
}

int SwitcherViewBase::buttonIndex(const SwitcherButton *button) const
{
    if(button == NULL) {
        return -1;
    }

    QList<QSharedPointer<SwitcherButton> > buttons = model()->buttons();
    for (int i = 0; i < buttons.count(); ++i) {
        if (buttons.at(i) == button) {
            return i;
        }
    }

    return -1;
}

void SwitcherViewBase::calculateNearestButtonAt(const QPointF &centerPoint)
{
    float minDistance = FLT_MAX;
    SwitcherButton *closestButton = NULL;

    foreach (const QSharedPointer<SwitcherButton> &button, model()->buttons()) {
        QLineF vec(centerPoint, button->mapToItem(controller, button->rect().center()));

        float distance = vec.length();

        if(distance < minDistance) {
            minDistance   = distance;
            closestButton = button.data();
        }
    }

    pinchedButtonPosition = buttonIndex(closestButton);
}

void SwitcherViewBase::pinchBegin(const QPointF &centerPoint)
{
    // Send cancel event to all items below, to prevent panning during the pinch
    MScene *scene = MainWindow::instance()->scene();
    QList<QGraphicsItem*> items = scene->items(controller->mapToScene(controller->rect().center()));
    MCancelEvent cancelEvent;
    foreach(QGraphicsItem *item, items) {
        scene->sendEvent(item, &cancelEvent);
    }

    calculateNearestButtonAt(centerPoint);

    foreach(const QSharedPointer<SwitcherButton> &button, model()->buttons()) {
        button->installSceneEventFilter(controller);
    }

    MainWindow::instance()->setOrientationLocked(true);
}

void SwitcherViewBase::pinchUpdate(float scaleFactor)
{
    if(!layoutAnimation->isAnimating()) {
        pinchGestureTargetMode = scaleFactor >= 1 ? SwitcherModel::Detailview : SwitcherModel::Overview;

        overpinch = pinchGestureTargetMode == model()->switcherMode();

        // Switch the mode and start the transition if needed
        if(model()->switcherMode() != pinchGestureTargetMode) {
            layoutAnimation->setManualControl(true);
            layoutAnimation->start();
            applyPinchGestureTargetMode();
        }
    }

    // Calculate the current animation progress based on the current scale factor
    qreal p = pinchGestureTargetMode == SwitcherModel::Detailview ?
              (scaleFactor - 1.0f) : (1.0f - scaleFactor);

    p = qBound(qreal(0.0), p * style()->pinchLength(), qreal(1));

    if(overpinch) {
        if(bounceAnimation->state() == QAbstractAnimation::Stopped) {
            setInwardBounceAnimation(model()->switcherMode() == SwitcherModel::Overview);
            bounceAnimation->setDirection(QAbstractAnimation::Forward);
            startBounceAnimation();
            bounceAnimation->pause();
        }

        bounceAnimation->setCurrentTime(p * bounceAnimation->duration() / 2);
    } else {
        layoutAnimation->setProgress(p);
    }
}

void SwitcherViewBase::pinchEnd()
{
    layoutAnimation->setManualControl(false);

    if(bounceAnimation->state() == QAbstractAnimation::Paused) {
        bounceAnimation->setDirection(QAbstractAnimation::Backward);
        bounceAnimation->resume();
    }

    // Cancel the transition if the pinch value plus twice the current pinching speed is less or equal to the threshold
    if(layoutAnimation->currentCurveValue() + layoutAnimation->speed() * 2.0f <= style()->pinchCancelThreshold()) {
        pinchGestureTargetMode = pinchGestureTargetMode == SwitcherModel::Detailview ? SwitcherModel::Overview : SwitcherModel::Detailview;
        layoutAnimation->cancelAnimation();
    }

    foreach (const QSharedPointer<SwitcherButton> &button, model()->buttons()) {
        button->setDown(false);
    }
}

void SwitcherViewBase::pinchGestureEvent(QGestureEvent *event, QPinchGesture *gesture)
{
    /*! Finish any currently running animation before starting a new one */
    if((layoutAnimation->isAnimating() && !layoutAnimation->manualControl()) || bounceAnimation->state() == QAbstractAnimation::Running) {
        return;
    }

    event->accept(gesture);

    switch(gesture->state()) {
    case Qt::GestureStarted:
        pinchBegin(controller->mapFromScene(gesture->centerPoint()));
        break;
    case Qt::GestureUpdated:
        pinchUpdate(gesture->totalScaleFactor());
        break;
    case Qt::GestureFinished:
        pinchEnd();
        break;
    default:
        break;
    }
}

bool SwitcherViewBase::sceneEventFilter(QGraphicsItem *watched, QEvent *event)
{
    bool filtered = false;

    if(event->type() == QEvent::GraphicsSceneMouseMove) {
        foreach(const QSharedPointer<SwitcherButton> &button, model()->buttons()) {
            if(button == watched) {
                filtered = true;
                break;
            }
        }
    }

    return filtered;
}

void SwitcherViewBase::endTransition()
{
    MainWindow::instance()->setOrientationLocked(false);

    if(layoutAnimation->isCanceled()) {
        applyPinchGestureTargetMode();
    }

    layoutAnimation->stop();
}

void SwitcherViewBase::endBounce()
{
    MainWindow::instance()->setOrientationLocked(false);
}

void SwitcherViewBase::applyPinchGestureTargetMode()
{
    model()->setSwitcherMode(pinchGestureTargetMode);
}

void SwitcherViewBase::setInwardBounceAnimation(bool i)
{
    // set the middle key value to either less than 1 when bouncing or zooming in overview mode,
    // or over 1 when zooming in detail mode
    bounceAnimation->setKeyValueAt(0.5f, 1.0f + (i ? -1.0f : 1.0f) * style()->bounceScale());
}

void SwitcherViewBase::startBounceAnimation()
{
    pannedWidget->setTransformOriginPoint(pannedWidget->mapFromParent(pannedWidget->parentWidget()->rect().center()));

    bounceAnimation->setDuration(style()->bounceDuration());
    bounceAnimation->setEasingCurve(style()->bounceCurve());
    bounceAnimation->start();
}

void SwitcherViewBase::runOverviewBounceAnimation()
{
    if(pinchGestureTargetMode == SwitcherModel::Overview) {
        setInwardBounceAnimation(true);
        startBounceAnimation();
    }
}

void SwitcherViewBase::updateAnimationStatus()
{
}

M_REGISTER_VIEW_NEW(SwitcherViewBase, Switcher)
