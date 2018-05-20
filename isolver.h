#ifndef _ISOLVER_H_
#define _ISOLVER_H_

class ISolver
{
public:
  virtual ~ISolver() {};

  auto virtual findSolution() -> void = 0;
  auto virtual stopFinding() -> void = 0;

  auto virtual getHashrate() const -> double const = 0;

  auto virtual updateTarget() -> void = 0;
  auto virtual updateMessage() -> void = 0;
};

#endif // !_ISOLVER_H_
