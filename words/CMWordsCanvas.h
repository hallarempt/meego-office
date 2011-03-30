#ifndef CALLIGRAMOBILE_WORDSCANVAS_H
#define CALLIGRAMOBILE_WORDSCANVAS_H

#include "../CMCanvasControllerDeclarative.h"

class CMWordsCanvas : public CMCanvasControllerDeclarative
{
    Q_OBJECT
    Q_PROPERTY(QObject* document READ doc)
    Q_PROPERTY(int page READ page WRITE changePage NOTIFY pageChanged)
public:
    CMWordsCanvas(QDeclarativeItem* parent = 0);
    virtual ~CMWordsCanvas();

    QObject* doc() const;
    int page() const;

public Q_SLOTS:
    void loadDocument();
    void changePage(int newPage);

Q_SIGNALS:
    void pageChanged(int newPage);

private:
    class Private;
    Private * const d;
};

#endif // CALLIGRAMOBILE_WORDSCANVAS_H
