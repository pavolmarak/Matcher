#include "matcher.h"

Matcher::Matcher()
{
    this->matcherIsRunning = false;
    this->matcher = suprema;
    this->thresholds = {50, 0.15};

    this->dbtestParams.numberOfSubject = 0;
    this->dbtestParams.imgPerSubject = 0;
    this->dbtestParams.genuineTestDone = false;

    this->dbtestResult.plotParams = {0, 500, 1};
    this->supremaMatcher.loaded = false;

    connect(&this->bozorth3m, SIGNAL(bozorthThreadsFinished(int)), this, SLOT(bozorthMatchingDone(int)));
}

Matcher::~Matcher()
{
    if (this->supremaMatcher.loaded) UFM_Delete(this->supremaMatcher.matcher);
}

void Matcher::cleanDBTestResults()
{
    this->dbtestParams.genuineTestDone = false;
    this->dbtestResult.fnmrX.clear(); this->dbtestResult.fnmrY.clear();
    this->dbtestResult.fmrX.clear(); this->dbtestResult.fmrY.clear();
    this->dbtestResult.rocX.clear(); this->dbtestResult.rocY.clear();
    this->dbtestResult.eer = -1;
    this->dbtestParams.keys.clear();
}

// SETTERS

int Matcher::setMatcher(MATCHER matcher)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return -1;
    }

    this->matcher = matcher;

    if (matcher == bozorth3) {
        this->dbtestResult.plotParams = {0, 500, 1};

        if (this->supremaMatcher.loaded) {
            UFM_Delete(this->supremaMatcher.matcher);
            this->supremaMatcher.loaded = false;
        }
    }
    else if (matcher == suprema) {
        this->dbtestResult.plotParams = {0, 1, 0.0001};

        UFM_STATUS ufm_res;

        ufm_res = UFM_Create(&this->supremaMatcher.matcher);
        if (ufm_res != 0) {
            this->matcherError(40); // You have to connect senyor first
            return -1;
        }
        else this->supremaMatcher.loaded = true;

        ufm_res = UFM_SetTemplateType(this->supremaMatcher.matcher, UFM_TEMPLATE_TYPE_ISO19794_2);
        int fastMode = 0;
        ufm_res = UFM_SetParameter(this->supremaMatcher.matcher, UFM_PARAM_FAST_MODE, &fastMode);
        int securityLevel = 4;
        ufm_res = UFM_SetParameter(this->supremaMatcher.matcher, UFM_PARAM_SECURITY_LEVEL, &securityLevel);
        int rotateMode = 1;
        ufm_res = UFM_SetParameter(this->supremaMatcher.matcher, UFM_PARAM_AUTO_ROTATE, &rotateMode);
        int sif = 1;
        ufm_res = UFM_SetParameter(this->supremaMatcher.matcher, UFM_PARAM_USE_SIF, &sif);
    }

    return 1;
}

void Matcher::setBozorthThreshold(float score)
{
    this->thresholds.bozorthThr = score;
}

void Matcher::setSupremaThreshold(float score)
{
    this->thresholds.supremaThr = score;
}

int Matcher::setDBTestParams(int numberOfSubject, int imgPerSubject)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return -1;
    }

    this->dbtestParams.numberOfSubject = numberOfSubject;
    this->dbtestParams.imgPerSubject = imgPerSubject;

    return 1;
}


// PAIR GENERATORS

void Matcher::generatePairs()
{
    this->fingerprintPairs.clear();

    for (auto i = this->alternativeNames.begin(); i != this->alternativeNames.end(); ++i) {
        this->fingerprintPairs.push_back(FINGERPRINT_PAIR{"SUBJECT", i.key(), 0});
    }
}

void Matcher::generateGenuinePairs()
{
    this->dbtestParams.genuinePairs.clear();

    for (int subject = 0; subject < this->dbtestParams.numberOfSubject; subject++) {
        for(int image1 = subject * this->dbtestParams.imgPerSubject; image1 < subject * this->dbtestParams.imgPerSubject + this->dbtestParams.imgPerSubject; image1++) {
            for(int image2 = image1+1; image2 < subject * this->dbtestParams.imgPerSubject + this->dbtestParams.imgPerSubject; image2++) {

                this->dbtestParams.genuinePairs.push_back(FINGERPRINT_PAIR{this->dbtestParams.keys[image1], this->dbtestParams.keys[image2], 0});

            }
        }
    }
}

void Matcher::generateImpostorPairs()
{   
    this->dbtestParams.impostorPairs.clear();

    for (int image1 = 0; image1 < (this->dbtestParams.numberOfSubject-1) * this->dbtestParams.imgPerSubject; image1 += this->dbtestParams.imgPerSubject) {
        for (int image2 = image1 + this->dbtestParams.imgPerSubject; image2 < this->dbtestParams.numberOfSubject * this->dbtestParams.imgPerSubject; image2 += this->dbtestParams.imgPerSubject) {

            this->dbtestParams.impostorPairs.push_back(FINGERPRINT_PAIR{this->dbtestParams.keys[image1], this->dbtestParams.keys[image2], 0});

        }
    }
}





//IDENTIFICATION, VERIFICATION, DBTEST

void Matcher::identify(unsigned char* subjectISO, const QMultiMap<QString, unsigned char*> &dbISO)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return;
    }
    else this->matcherIsRunning = true;

    this->mode = identification;

    if (this->matcher == bozorth3) {
        //ALTERNATIVE_NAMES, BOZORTH TEMPLATES
        this->alternativeNames.clear();
        this->bozorthTemplates.clear();

        this->bozorthTemplates.insert("SUBJECT", this->isoConverter.convertFromISO(subjectISO));

        for (auto i = dbISO.begin(); i != dbISO.end(); ++i) {
            if (this->alternativeNames.contains(i.key())) {
                int cnt = 0;
                while (this->alternativeNames.contains(i.key() + "_" + QString::number(cnt))) {
                    cnt++;
                }
                this->alternativeNames.insert(i.key() + "_" + QString::number(cnt), i.key());
                this->bozorthTemplates.insert(i.key() + "_" + QString::number(cnt), this->isoConverter.convertFromISO(i.value()));
            }
            else {
                this->alternativeNames.insert(i.key(), i.key());
                this->bozorthTemplates.insert(i.key(), this->isoConverter.convertFromISO(i.value()));
            }
        }
        this->generatePairs();
        this->bozorth3m.setParameters(QThread::idealThreadCount(), this->bozorthTemplates, this->fingerprintPairs);
        this->bozorth3m.matchAll();
    }
    else if (this->matcher == suprema) {

        float score;
        int success;

        this->supremaMatcher.scores.clear();
        int subjectISOTemplateSize = this->isoConverter.getTemplateSize(subjectISO);

        int cnt = 1;
        for (auto i = dbISO.begin(); i != dbISO.end(); ++i) {
            this->dbtestParams.keys.push_back(i.key());
            UFM_VerifyEx(this->supremaMatcher.matcher, subjectISO, subjectISOTemplateSize, i.value(), this->isoConverter.getTemplateSize(i.value()), &score, &success);
            this->supremaMatcher.scores.push_back(score);

            emit matcherProgressSignal((int)(cnt++ * 1.0 / dbISO.size() * 100));
        }

        this->supremaMatchingDone();
    }
}

void Matcher::identify(const QVector<MINUTIA> &subject, const QMultiMap<QString, QVector<MINUTIA>> &db)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return;
    }
    else this->matcherIsRunning = true;

    this->mode = identification;

    if (this->matcher == bozorth3) {
        //ALTERNATIVE_NAMES, BOZORTH TEMPLATES
        this->alternativeNames.clear();
        this->bozorthTemplates.clear();

        this->bozorthTemplates.insert("SUBJECT", subject);

        for (auto i = db.begin(); i != db.end(); ++i) {
            if (this->alternativeNames.contains(i.key())) {
                int cnt = 0;
                while (this->alternativeNames.contains(i.key() + "_" + QString::number(cnt))) {
                    cnt++;
                }
                this->alternativeNames.insert(i.key() + "_" + QString::number(cnt), i.key());
                this->bozorthTemplates.insert(i.key() + "_" + QString::number(cnt), i.value());
            }
            else {
                this->alternativeNames.insert(i.key(), i.key());
                this->bozorthTemplates.insert(i.key(), i.value());
            }
        }
        this->generatePairs();
        this->bozorth3m.setParameters(QThread::idealThreadCount(), this->bozorthTemplates, this->fingerprintPairs);
        this->bozorth3m.matchAll();
    }
    else if (this->matcher == suprema) {

        float score;
        int success;

        this->supremaMatcher.scores.clear();

        unsigned char * isoTemplate;
        unsigned char * subjectISO;

        this->isoConverter.load(subject[0].imgWH.y(), subject[0].imgWH.x(), 100, subject);
        subjectISO = this->isoConverter.convertToISO();

        int cnt = 1;
        for (auto i = db.begin(); i != db.end(); ++i) {
            this->dbtestParams.keys.push_back(i.key());

            this->isoConverter.load(i.value()[0].imgWH.y(), i.value()[0].imgWH.x(), 100, i.value());
            isoTemplate = this->isoConverter.convertToISO();

            UFM_VerifyEx(this->supremaMatcher.matcher, subjectISO, this->isoConverter.getTemplateSize(subjectISO), isoTemplate, this->isoConverter.getTemplateSize(isoTemplate), &score, &success);
            this->supremaMatcher.scores.push_back(score);

            emit matcherProgressSignal((int)(cnt++ * 1.0 / db.size() * 100));
        }
        delete isoTemplate;
        delete subjectISO;

        this->supremaMatchingDone();
    }
}

void Matcher::verify(unsigned char* subjectISO, const QVector<unsigned char *> &dbISO)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return;
    }
    else this->matcherIsRunning = true;

    this->mode = verification;

    if (this->matcher == bozorth3) {
        // ALTERNATIVE NAMES
        this->alternativeNames.clear();
        this->bozorthTemplates.clear();

        this->bozorthTemplates.insert("SUBJECT", this->isoConverter.convertFromISO(subjectISO));

        for (int i = 0; i < dbISO.size(); i++) {
            this->alternativeNames.insert(QString::number(i), QString::number(i));
            this->bozorthTemplates.insert(QString::number(i), this->isoConverter.convertFromISO(dbISO[i]));
        }
        this->generatePairs();
        this->bozorth3m.setParameters(QThread::idealThreadCount(), this->bozorthTemplates, this->fingerprintPairs);
        this->bozorth3m.matchAll();
    }
    else if (this->matcher == suprema) {

        float score;
        int success;

        this->supremaMatcher.scores.clear();
        int subjectISOTemplateSize = this->isoConverter.getTemplateSize(subjectISO);

        for (int i = 0; i < dbISO.size(); i++) {
            UFM_VerifyEx(this->supremaMatcher.matcher, subjectISO, subjectISOTemplateSize, dbISO.at(i), this->isoConverter.getTemplateSize(dbISO.at(i)), &score, &success);
            this->supremaMatcher.scores.push_back(score);

            emit matcherProgressSignal((int)(i * 1.0/ (dbISO.size()-1) * 100));
        }

        this->supremaMatchingDone();
    }
}

void Matcher::verify(const QVector<MINUTIA> &subject, const QVector<QVector<MINUTIA> > &db)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return;
    }
    else this->matcherIsRunning = true;

    this->mode = verification;

    if (this->matcher == bozorth3) {
        // ALTERNATIVE NAMES
        this->alternativeNames.clear();
        this->bozorthTemplates.clear();

        this->bozorthTemplates.insert("SUBJECT", subject);

        for (int i = 0; i < db.size(); i++) {
            this->alternativeNames.insert(QString::number(i), QString::number(i));
            this->bozorthTemplates.insert(QString::number(i), db[i]);
        }
        this->generatePairs();
        this->bozorth3m.setParameters(QThread::idealThreadCount(), this->bozorthTemplates, this->fingerprintPairs);
        this->bozorth3m.matchAll();
    }
    else if (this->matcher == suprema) {

        float score;
        int success;

        this->supremaMatcher.scores.clear();

        unsigned char * isoTemplate;
        unsigned char * subjectISO;

        this->isoConverter.load(subject[0].imgWH.y(), subject[0].imgWH.x(), 100, subject);
        subjectISO = this->isoConverter.convertToISO();

        for (int i = 0; i < db.size(); i++) {
            this->isoConverter.load(db[i].at(0).imgWH.y(), db[i].at(0).imgWH.x(), 100, db[i]);
            isoTemplate = this->isoConverter.convertToISO();

            UFM_VerifyEx(this->supremaMatcher.matcher, subjectISO, this->isoConverter.getTemplateSize(subjectISO), isoTemplate, this->isoConverter.getTemplateSize(isoTemplate), &score, &success);
            this->supremaMatcher.scores.push_back(score);

            emit matcherProgressSignal((int)(i * 1.0/ (db.size()-1) * 100));
        }
        delete isoTemplate;
        delete subjectISO;

        this->supremaMatchingDone();
    }
}

void Matcher::testDatabase(QMap<QString, QVector<MINUTIA>> &db)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return;
    }
    else this->matcherIsRunning = true;

    this->mode = dbtest;

    if (this->matcher == bozorth3) {
        // GENUINES
        this->bozorthTemplates.clear();

        for (auto i = db.begin(); i != db.end(); ++i) {
            this->dbtestParams.keys.push_back(i.key());
            //this->boostMinutiae(i.value(), 60);
            this->bozorthTemplates.insert(i.key(), i.value());
        }
        this->generateGenuinePairs();
        this->generateImpostorPairs();

        this->bozorth3m.setParameters(QThread::idealThreadCount(), this->bozorthTemplates, this->dbtestParams.genuinePairs);
        this->bozorth3m.matchAll();
    }
    else if (this->matcher == suprema) {

        for (auto i = db.begin(); i != db.end(); ++i) {
            this->dbtestParams.keys.push_back(i.key());
        }

        this->generateGenuinePairs();
        this->generateImpostorPairs();

        float score;
        int success;
        int cnt = 0;

        unsigned char * ISOTemplate1;
        unsigned char * ISOTemplate2;

        // GENUINES
        for (FINGERPRINT_PAIR &i : this->dbtestParams.genuinePairs) {
            this->isoConverter.load(db.value(i.leftFingerprint)[0].imgWH.y(), db.value(i.leftFingerprint)[0].imgWH.x(), 100, db.value(i.leftFingerprint));
            ISOTemplate1 = this->isoConverter.convertToISO();
            this->isoConverter.load(db.value(i.rightFingerprint)[0].imgWH.y(), db.value(i.rightFingerprint)[0].imgWH.x(), 100, db.value(i.rightFingerprint));
            ISOTemplate2 = this->isoConverter.convertToISO();

            UFM_VerifyEx(this->supremaMatcher.matcher, ISOTemplate1, this->isoConverter.getTemplateSize(ISOTemplate1), ISOTemplate2, this->isoConverter.getTemplateSize(ISOTemplate2), &score, &success);
            i.score = score;

            delete ISOTemplate1;
            delete ISOTemplate2;

            emit matcherProgressSignal((int)(cnt++ * 1.0/ (this->dbtestParams.genuinePairs.size() + this->dbtestParams.impostorPairs.size() - 2) * 100));
        }

        // IMPOSTORS
        for (FINGERPRINT_PAIR &i : this->dbtestParams.impostorPairs) {
            this->isoConverter.load(db.value(i.leftFingerprint)[0].imgWH.y(), db.value(i.leftFingerprint)[0].imgWH.x(), 100, db.value(i.leftFingerprint));
            ISOTemplate1 = this->isoConverter.convertToISO();
            this->isoConverter.load(db.value(i.rightFingerprint)[0].imgWH.y(), db.value(i.rightFingerprint)[0].imgWH.x(), 100, db.value(i.rightFingerprint));
            ISOTemplate2 = this->isoConverter.convertToISO();

            UFM_VerifyEx(this->supremaMatcher.matcher, ISOTemplate1, this->isoConverter.getTemplateSize(ISOTemplate1), ISOTemplate2, this->isoConverter.getTemplateSize(ISOTemplate2), &score, &success);
            i.score = score;

            delete ISOTemplate1;
            delete ISOTemplate2;

            emit matcherProgressSignal((int)(cnt++ * 1.0/ (this->dbtestParams.genuinePairs.size() + this->dbtestParams.impostorPairs.size() - 2) * 100));
        }

        this->supremaMatchingDone();


        // INPUT SHOULD BE IN ISO FORMAT
        this->matcherError(11);
        return;
    }
}

void Matcher::testDatabase(const QMap<QString, unsigned char *> &dbISO)
{
    if (this->matcherIsRunning) {
        this->matcherError(10);
        return;
    }
    else this->matcherIsRunning = true;

    this->mode = dbtest;

    if (this->matcher == suprema) {
        for (auto i = dbISO.begin(); i != dbISO.end(); ++i) {
            this->dbtestParams.keys.push_back(i.key());
        }

        this->generateGenuinePairs();
        this->generateImpostorPairs();

        float score;
        int success;
        int cnt = 0;

        // GENUINES
        for (FINGERPRINT_PAIR &i : this->dbtestParams.genuinePairs) {
            UFM_VerifyEx(this->supremaMatcher.matcher, dbISO.value(i.leftFingerprint), this->isoConverter.getTemplateSize(dbISO.value(i.leftFingerprint)), dbISO.value(i.rightFingerprint), this->isoConverter.getTemplateSize(dbISO.value(i.rightFingerprint)), &score, &success);
            i.score = score;
            emit matcherProgressSignal((int)(cnt++ * 1.0/ (this->dbtestParams.genuinePairs.size() + this->dbtestParams.impostorPairs.size() - 2) * 100));
        }

        // IMPOSTORS
        for (FINGERPRINT_PAIR &i : this->dbtestParams.impostorPairs) {
            UFM_VerifyEx(this->supremaMatcher.matcher, dbISO.value(i.leftFingerprint), this->isoConverter.getTemplateSize(dbISO.value(i.leftFingerprint)), dbISO.value(i.rightFingerprint), this->isoConverter.getTemplateSize(dbISO.value(i.rightFingerprint)), &score, &success);
            i.score = score;

            emit matcherProgressSignal((int)(cnt++ * 1.0/ (this->dbtestParams.genuinePairs.size() + this->dbtestParams.impostorPairs.size() - 2) * 100));
        }

        this->supremaMatchingDone();
    }
    else if (this->matcher == bozorth3) {
        this->dbtestParams.db.clear();

        for (auto i = dbISO.begin(); i != dbISO.end(); ++i) {
            this->dbtestParams.db.insert(i.key(), this->isoConverter.convertFromISO(i.value()));
        }

        this->matcherIsRunning = false;
        this->testDatabase(this->dbtestParams.db);
    }
}

SUPREMA_MATCHER Matcher::getSupremaMatcher() const
{
    return supremaMatcher;
}

DBTEST_RESULT Matcher::getDbtestResult() const
{
    return dbtestResult;
}




//SLOTS
void Matcher::bozorthMatchingDone(int duration)
{
    if (this->mode == identification) {
        this->fingerprintPairs = this->bozorth3m.getOutputFingerprintPairs();

        int bestMatch = this->findMaxScoreItem();
        if (this->fingerprintPairs[bestMatch].score >= this->thresholds.bozorthThr)
            emit identificationDoneSignal(true, this->alternativeNames.value(this->fingerprintPairs[bestMatch].rightFingerprint), this->fingerprintPairs[bestMatch].score);
        else emit identificationDoneSignal(false, this->alternativeNames.value(this->fingerprintPairs[bestMatch].rightFingerprint), this->fingerprintPairs[bestMatch].score);

        this->matcherIsRunning = false;
    }
    else if (this->mode == verification) {
        this->fingerprintPairs = this->bozorth3m.getOutputFingerprintPairs();

        int bestMatch = this->findMaxScoreItem();
        if (this->fingerprintPairs[bestMatch].score >= this->thresholds.bozorthThr)
            emit verificationDoneSignal(true,this->fingerprintPairs[bestMatch].score);
        else emit verificationDoneSignal(false,this->fingerprintPairs[bestMatch].score);

        this->matcherIsRunning = false;
    }
    else if (this->mode == dbtest) {

        double error;

        if (!this->dbtestParams.genuineTestDone) {
            this->dbtestParams.genuinePairs = this->bozorth3m.getOutputFingerprintPairs();

            for(int threshold = this->dbtestResult.plotParams.min; threshold < this->dbtestResult.plotParams.max; threshold += this->dbtestResult.plotParams.sensitivity) {
                this->dbtestResult.fnmrX.push_back(threshold);
                error = std::count_if(this->dbtestParams.genuinePairs.begin(), this->dbtestParams.genuinePairs.end(), [=](FINGERPRINT_PAIR fp) {return fp.score < threshold;});
                this->dbtestResult.fnmrY.push_back(error/this->dbtestParams.genuinePairs.size()*100);
            }
            this->dbtestParams.genuineTestDone = true;

            emit matcherProgressSignal(50);

            // IMPOSTORS
            this->bozorth3m.setParameters(QThread::idealThreadCount(), this->bozorthTemplates, this->dbtestParams.impostorPairs);
            this->bozorth3m.matchAll();
        }
        else {

            this->dbtestParams.impostorPairs = this->bozorth3m.getOutputFingerprintPairs();
            for(int threshold = this->dbtestResult.plotParams.min; threshold < this->dbtestResult.plotParams.max; threshold += this->dbtestResult.plotParams.sensitivity) {
                this->dbtestResult.fmrX.push_back(threshold);
                error = std::count_if(this->dbtestParams.impostorPairs.begin(), this->dbtestParams.impostorPairs.end(), [=](FINGERPRINT_PAIR fp) {return fp.score >= threshold;});
                this->dbtestResult.fmrY.push_back(error/this->dbtestParams.impostorPairs.size()*100);
            }
            this->compureROCValues();
            this->dbtestResult.eer = this->computeEERValue();

            emit matcherProgressSignal(100);
            emit dbTestDoneSignal(dbtestResult);

            this->matcherIsRunning = false;
            this->cleanDBTestResults();
        }
    }
}

void Matcher::supremaMatchingDone()
{
    if (this->mode == identification) {

        int bestMatch = std::distance(this->supremaMatcher.scores.begin(), std::max_element(this->supremaMatcher.scores.begin(), this->supremaMatcher.scores.end()));
        if (this->supremaMatcher.scores[bestMatch] >= this->thresholds.supremaThr)
            emit identificationDoneSignal(true, this->dbtestParams.keys[bestMatch], this->supremaMatcher.scores[bestMatch]);
        else emit identificationDoneSignal(false, this->dbtestParams.keys[bestMatch], this->supremaMatcher.scores[bestMatch]);

        this->matcherIsRunning = false;
    }
    else if (this->mode == verification) {
        int bestMatch = std::distance(this->supremaMatcher.scores.begin(), std::max_element(this->supremaMatcher.scores.begin(), this->supremaMatcher.scores.end()));
        if (this->supremaMatcher.scores[bestMatch] >= this->thresholds.supremaThr)
            emit verificationDoneSignal(true, this->supremaMatcher.scores[bestMatch]);
        else emit verificationDoneSignal(false, this->supremaMatcher.scores[bestMatch]);

        this->matcherIsRunning = false;
    }
    else if (this->mode == dbtest) {

        double error;

        for(double threshold = this->dbtestResult.plotParams.min; threshold < this->dbtestResult.plotParams.max; threshold += this->dbtestResult.plotParams.sensitivity) {
            this->dbtestResult.fnmrX.push_back(threshold);
            error = std::count_if(this->dbtestParams.genuinePairs.begin(), this->dbtestParams.genuinePairs.end(), [=](FINGERPRINT_PAIR fp) {return fp.score < threshold;});
            this->dbtestResult.fnmrY.push_back(error/this->dbtestParams.genuinePairs.size()*100);
        }

        for(double threshold = this->dbtestResult.plotParams.min; threshold < this->dbtestResult.plotParams.max; threshold += this->dbtestResult.plotParams.sensitivity) {
            this->dbtestResult.fmrX.push_back(threshold);
            error = std::count_if(this->dbtestParams.impostorPairs.begin(), this->dbtestParams.impostorPairs.end(), [=](FINGERPRINT_PAIR fp) {return fp.score >= threshold;});
            this->dbtestResult.fmrY.push_back(error/this->dbtestParams.impostorPairs.size()*100);
        }
        this->compureROCValues();
        this->dbtestResult.eer = this->computeEERValue();

        emit dbTestDoneSignal(dbtestResult);

        this->matcherIsRunning = false;
        this->cleanDBTestResults();
    }
}

// OTHER
int Matcher::findMaxScoreItem()
{
    int maxItemNum = 0;
    float max = this->fingerprintPairs[0].score;

    for (int i = 1; i < this->fingerprintPairs.size(); i++) {
        if (this->fingerprintPairs[i].score > max) maxItemNum = i;
    }

    return maxItemNum;
}

double Matcher::computeEERValue()
{
    QVector<double> absDiff;
    for(int i=0; i < this->dbtestResult.fmrY.size(); i++){
        absDiff.push_back(qAbs(this->dbtestResult.fmrY[i] - this->dbtestResult.fnmrY[i]));
    }
    double smallestDiff = *std::min_element(absDiff.begin(), absDiff.end());

    for(int i=0; i< absDiff.size(); i++){
        if(absDiff[i] == smallestDiff) {
            return (this->dbtestResult.fmrY[i] + this->dbtestResult.fnmrY[i])/2.0;
        }
    }

    return 0;
}

void Matcher::compureROCValues()
{
    foreach (double  i, this->dbtestResult.fnmrY) {
        this->dbtestResult.rocY.push_back(1-i/100.0);
    }

    foreach (double  i, this->dbtestResult.fmrY) {
        this->dbtestResult.rocX.push_back(i/100.0);
    }
}

void Matcher::boostMinutiae(QVector<MINUTIA> &mv, int minMinutiae)
{
    qSort(mv.begin(), mv.end(),[=](const MINUTIA& left, const MINUTIA& right){return left.quality > right.quality;});

    int cnt = 0;
    int origSize = mv.size();
    while (mv[cnt].quality > 79 && mv.size() != 2*origSize) {
        mv.push_back(mv[cnt++]);
    }
}

// ERROR
void Matcher::matcherError(int errorCode)
{
    /* Error codes:
     *
     *
     *
    */

    emit this->matcherErrorSignal(errorCode);
}
