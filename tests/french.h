#pragma once

#include <QCoreApplication>
#include <QTranslator>

// TuxSCR in French, as TuxAPE starts it when its windows are in French: the
// plugin's own translation (the one built into it, TUXSCR_FRENCH) installed
// for as long as this lives. The windows made meanwhile speak French; those
// made before keep the language they were made in, as in TuxAPE, which
// starts again to change its language.
class French {
public:
    French()
    {
        loaded_ = translator_.load(QStringLiteral(TUXSCR_FRENCH));
        if (loaded_)
            QCoreApplication::installTranslator(&translator_);
    }
    ~French() { QCoreApplication::removeTranslator(&translator_); }
    French(const French&) = delete;
    French& operator=(const French&) = delete;

    bool loaded() const { return loaded_; }

private:
    QTranslator translator_;
    bool loaded_ = false;
};
