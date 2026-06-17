# Documentation and Features

Experimental nightly builds only for now. No Mac builds yet. This is due to
needing OS 14 to get a build server queue, and that server getting a 302 redirect,
followed by a 404 on the Rack SDK download for the github build server.

This repository contains modules that I like, but are often dormant with some good
ideas in the issues tab. It's not to say that other modules aren't availablein the
[VCV Rack Plugin Directory](https://vcvrack.com/plugins), but this repository is a
collection of modules that have been developed further to be more useful. Apparent
Simplicity is often the key to a good module.

# VCV Rack Modules by Varoius People

## Frank Buss

![Formula](docs/formula.png "Formula")

[Formula for CV and audio](docs/formula.md)

### Extras

- [X] `c` is for channel number (1 to 16) ... not cookie?
- [X] `m` is for set poly out (1 to 16) ... ish!
- [X] `f` is for frequency (delayed by one sample)
- [X] `l` is for lowpass filter (delayed by one sample, `f` relative)
- [X] `s` is for suboscillation multiplier (for unipolar signals like `par(p)`)
- [X] `r` is for sample rate
- [X] `par(p)` is for parabolic phase `4*p*(1-p)`
- [X] queing and unquing FIFO to `PORT_MAX_CHANNELS` (crosstalk)
  - [X] `que(x)` is for queuing (`que` evaluates to `x`)
  - [X] `unq(i)` is for unqueueing (index `i` is tail offset, 0 for tail)
- [X] `expm1(x)` is for `exp(x) - 1` (zero-er DC audio bias)
- [X] `log1p(x)` is for `log(1 + x)` (zero-er DC audio bias)
- [X] `nor(gain)` is for normal Gaussian noise
- [X] `uni(gain)` is for uniform distribution (0 to `gain`)
- [X] `exd(gain)` is for exponential distribution
- [X] `poi(gain)` is for Poisson distribution
- [X] `tanp(x)` is for `tan(pi*x)` by fast Pade approximation (5/5)
- [X] `sinp(x)` is for `sin(pi*x)` by fast Pade approximation (5/5)
- [X] `cosp(x)` is for `cos(pi*x)` by fast Pade approximation (5/5)
- [X] `cbrt(x)` is for cube root (a favorite distortion)
- [X] added monadic `/` (monadic `/` is for prefix reciprocal)
- [X] fuzzy equality/inequality (within 1 V)
- [X] fuzzy `&` and `|`
- [X] diabled unnecessary exception code (`NaN`, `Inf` no `if`)
- [X] optimized and threaded
- [X] new front panel layout
- [X] 11 HP
- [ ] SIMD branch (compiles, no cigar, the black screen no seen)

![Unofficial Formula](docs/unofficial-formula.png "Unofficial Formula")

### Notes

This is a modulation source. Using `f` or `l` in the frequency formula (lower text box)
might sometimes be good, but can lead to very high frequencies at large amplitudes.
You are advised to place a lowpass filter after this module if you are listening
to the output directly, as the high-frequency artifacts can be very loud.

Quick shortcut names left: `a`, `d`, `g`, `h`, `i`, `j`, `n`, `o`, `q`, `t`, `u`, `v`.
Then it's capital letters: `A`, `B`, `C`, `D`, `E`, `F`, `G`, `H`, `I`, `J`, `K`, `L`, `M`, `N`, `O`, `P`, `Q`, `R`, `S`, `T`, `U`, `V`, `W`, `X`, `Y`, `Z` (or `_`).
