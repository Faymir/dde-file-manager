/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     Gary Wang <wzc782970009@gmail.com>
 *
 * Maintainer: Gary Wang <wangzichong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef DFMCRUMBBAR_H
#define DFMCRUMBBAR_H

#include <QWidget>

#include <dfmglobal.h>

class DFMEvent;

DFM_BEGIN_NAMESPACE

class DFMCrumbItem;
class DFMCrumbBarPrivate;
class DFMCrumbBar : public QWidget
{
    Q_OBJECT
public:
    explicit DFMCrumbBar(QWidget *parent = nullptr);
    ~DFMCrumbBar();

    void updateCrumbs(const DUrl &url);

private:
    QScopedPointer<DFMCrumbBarPrivate> d_ptr;

Q_SIGNALS:
    void toggleSearchBar();
    void crumbItemClicked(DFMCrumbItem *item);

protected:
    void mousePressEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *e) override;

    Q_DECLARE_PRIVATE(DFMCrumbBar)
};

DFM_END_NAMESPACE

#endif // DFMCRUMBBAR_H
