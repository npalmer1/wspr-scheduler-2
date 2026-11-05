#pragma once
// The receive window (C#: RXForm). Records each 2-minute cycle, runs wsprd, saves the decodes to
// wspr_rpt.received, shows them and optionally uploads them to wsprnet.org.
//
// Public methods keep the C# names in their comments so Form1 code ports across directly.
// Behaviour differences are listed in PORTING_NOTES.md ("RXForm").
#include "rxstore.h"

#include <QDateTime>
#include <QWidget>
#include <memory>

namespace Ui { class RXForm; }
class RxCycle;
class RxTableModel;
class QSortFilterProxyModel;
class WsprnetUpload;
class SpectrumForm;

class RxForm : public QWidget
{
    Q_OBJECT
public:
    explicit RxForm(QWidget *parent = nullptr);
    ~RxForm() override;

    // C#: set_header(call, serverName, db_user, db_pass, loc, audioDev, wsprdpath, ver, opsys)
    void setHeader(const QString &call, const DbSettings &db, const QString &locator, const QString &audioIn,
                   const QString &wsprdPath, const QString &version);
    // C#: change_header (same, without the version; does not re-read rxconfig)
    void changeHeader(const QString &call, const DbSettings &db, const QString &locator, const QString &audioIn,
                      const QString &wsprdPath);
    void setFrequency(const QString &mhz);                 // C#: set_frequency
    void setTime(const QString &time, const QString &lt);  // C#: set_time
    void setLabel(const QString &status);                  // C#: setLabel (errors: short text + pop-up)
    void updateLabels(const QString &locator);             // C#: updateLabels
    bool startReceive();                                   // C#: Start_Receive
    void setBlockDecodes(bool block);                      // C#: blockDecodes = ...
    void setStopUrl(bool stop) { m_stopUrl = stop; }       // C#: stopUrl = ...
    void setGain(float gain);                              // C#: gain = ...
    void setWsprdPath(const QString &dir);                 // C#: wsprdfilepath = ...
    bool setAudioDevice(const QString &name);
    void saveConfig();                                     // C#: Save_Config
    void clearLog();                                       // C#: ClearLog
    bool useDeep() const;                                  // C#: useDeep (Form1.decodeDelay)
    int osd() const;                                       // C#: OSD
    bool receivingStopped() const;                         // stopRXcheckBox
    void setAllowClose(bool allow);                        // set by the main window on exit

    RxCycle *cycle() const { return m_cycle; }
    RxStore &store() { return m_store; }
    RxTableModel *model() const { return m_model; }
    SpectrumForm *spectrum() const { return m_spectrum.get(); }

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    // C# handler names
    void button1_Click();
    void filterbutton_Click();
    void Clearbutton_Click();
    void OSDlistBox_SelectedIndexChanged(int index);
    void DeepcheckBox_Clicked(bool checked);
    void QuickcheckBox_Clicked(bool checked);
    void kmcheckBox_CheckedChanged(bool checked);
    void stopRXcheckBox_CheckedChanged(bool checked);
    void cwssblistBox_SelectedValueChanged(const QString &text);
    void Spectrumbutton_Click();
    void rowHeaderContextMenu(const QPoint &pos);   // C#: dataGridView1_RowHeaderMouseClick (right button)

    void onDecoded(const Rx::DecodeResult &result, const QString &dial, const QDateTime &cycle);
    void showRows(const std::optional<QVector<RxSpot>> &rows);
    void updateDistanceLimits();
    void readConfig();                              // C#: Read_Config
    std::optional<int> distanceKm(const QString &text) const;

    std::unique_ptr<Ui::RXForm> ui;
    RxStore m_store;
    RxCycle *m_cycle;
    WsprnetUpload *m_upload;
    RxTableModel *m_model;
    QSortFilterProxyModel *m_proxy;
    std::unique_ptr<SpectrumForm> m_spectrum;   // own window, as C#
    QString m_call;
    QString m_loc;
    QString m_version;
    bool m_stopUrl = false;
    bool m_allowClose = false;
    bool m_loadingConfig = false;
    bool m_ready = false;          // setHeader called (database settings known)
    int m_maxRows = 2000;
    QString m_lastPopup;          // last error popped up, and when (not repeated within 10 minutes)
    QDateTime m_lastPopupAt;
};
